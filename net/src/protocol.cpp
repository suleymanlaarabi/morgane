#include "protocol.h"
#include <algorithm>

namespace net::detail {

bool read_header(std::span<const std::byte> bytes, Header &h) {
    if (bytes.size() < header_size || bytes.size() > mtu)
        return false;
    Reader r(bytes.first(header_size));
    if (r.get<std::uint32_t>() != magic || r.get<std::uint16_t>() != protocol_version)
        return false;
    h.flags = r.get<std::uint16_t>();
    h.session = r.get<std::uint64_t>();
    h.sequence = r.get<std::uint32_t>();
    h.ack = r.get<std::uint32_t>();
    h.ack_bits = r.get<std::uint64_t>();
    h.reliable_sequence = r.get<std::uint32_t>();
    h.payload_size = r.get<std::uint16_t>();
    return h.flags <= reliable_flag && h.sequence != 0 &&
           bool(h.flags & reliable_flag) == bool(h.reliable_sequence) &&
           h.payload_size == bytes.size() - header_size;
}

void write_header(Packet &packet, const Header &h) {
    Writer w(packet.data);
    w.put(magic);
    w.put(protocol_version);
    w.put(h.flags);
    w.put(h.session);
    w.put(h.sequence);
    w.put(h.ack);
    w.put(h.ack_bits);
    w.put(h.reliable_sequence);
    w.put(h.payload_size);
    packet.size = header_size + h.payload_size;
}

bool valid_frames(std::span<const std::byte> bytes) {
    Reader r(bytes);
    while (r.remaining()) {
        auto op = r.get<std::uint8_t>();
        auto size = r.get<std::uint16_t>();
        if (op > std::uint8_t(Op::event))
            return false;
        r.bytes(size);
        if (!r.valid())
            return false;
    }
    return r.done();
}

bool ReceiveWindow::contains(std::uint32_t sequence) const {
    if (latest == 0 || newer(sequence, latest))
        return false;
    auto distance = latest - sequence;
    return distance == 0 || distance > 64 || (bits & (std::uint64_t(1) << (distance - 1)));
}

void ReceiveWindow::accept(std::uint32_t sequence) {
    if (latest == 0) {
        latest = sequence;
        return;
    }
    if (newer(sequence, latest)) {
        auto distance = sequence - latest;
        bits = distance > 64 ? 0 : distance == 64 ? std::uint64_t(1) << 63
                                                   : (bits << distance) | (std::uint64_t(1) << (distance - 1));
        latest = sequence;
    } else if (auto distance = latest - sequence; distance && distance <= 64) {
        bits |= std::uint64_t(1) << (distance - 1);
    }
}

void Channel::acknowledge(const Header &header, Time now) {
    auto ack = [&](std::uint32_t sequence) {
        auto reliable = ack_lookup[sequence & 255];
        auto &record = sent[reliable & (reliable_window - 1)];
        if (!reliable || record.reliable_sequence != reliable || record.sequence != sequence)
            return;
        if (!record.retransmitted) {
            auto elapsed = std::chrono::duration<float, std::milli>(now - record.sent).count();
            rtt_ms += (elapsed - rtt_ms) * 0.125f;
        }
        record.reliable_sequence = 0;
        ack_lookup[sequence & 255] = 0;
        --in_flight;
    };
    if (header.ack)
        ack(header.ack);
    auto bits = header.ack_bits;
    while (bits) {
        auto distance = std::countr_zero(bits) + 1;
        ack(header.ack - distance);
        bits &= bits - 1;
    }
}

} // namespace net::detail
