#include "runtime.h"
#include <algorithm>

namespace net::detail {

std::size_t Runtime::schema_bytes(std::span<std::byte> target) const {
    Writer writer(target);
    writer.put(std::uint16_t(registry_->types.size()));
    for (std::size_t i = 0; i < registry_->types.size(); ++i) {
        writer.put(registry_->slots[i]->type_id);
        writer.put(registry_->slots[i]->schema);
    }
    return writer.size();
}

bool Runtime::schema_matches(Reader &reader) const {
    if (reader.get<std::uint16_t>() != registry_->types.size())
        return false;
    for (std::size_t i = 0; i < registry_->types.size(); ++i) {
        auto id = reader.get<std::uint64_t>();
        auto schema = reader.get<std::uint64_t>();
        if (id != registry_->slots[i]->type_id || schema != registry_->slots[i]->schema)
            return false;
    }
    return reader.valid();
}

bool Runtime::transmit(Peer &peer, std::span<const std::byte> payload, bool reliable, Time now) {
    auto &channel = peer.channel;
    auto sequence = channel.next_sequence;
    auto reliable_sequence = reliable ? channel.next_reliable : 0;
    auto &record = channel.sent[reliable_sequence & (reliable_window - 1)];
    if (reliable && (channel.in_flight == reliable_window || record.reliable_sequence ||
                     channel.ack_lookup[sequence & 255]))
        return false;
    auto *packet = transport_->send.writer();
    if (!packet)
        return false;
    packet->endpoint = peer.endpoint;
    std::copy(payload.begin(), payload.end(), packet->data.begin() + header_size);
    Header header{ .flags = reliable ? reliable_flag : std::uint16_t(0), .session = peer.session,
                   .sequence = sequence, .ack = channel.receive.latest, .ack_bits = channel.receive.bits,
                   .reliable_sequence = reliable_sequence, .payload_size = std::uint16_t(payload.size()) };
    write_header(*packet, header);
    if (reliable) {
        record.packet.copy_from(*packet);
        record.sequence = sequence;
        record.reliable_sequence = reliable_sequence;
        record.sent = now;
        record.retransmitted = false;
        channel.ack_lookup[sequence & 255] = reliable_sequence;
        ++channel.in_flight;
        ++channel.next_reliable;
    }
    ++channel.next_sequence;
    channel.ack_pending = false;
    peer.sent = now;
    peer.stats.sent_bytes += packet->size;
    ++peer.stats.sent_packets;
    transport_->publish();
    return true;
}

void Runtime::send_hello(Peer &peer, Time now) {
    auto size = schema_bytes(std::span(handshake_).subspan(3));
    Writer writer(handshake_);
    writer.put(std::uint8_t(Op::hello));
    writer.put(std::uint16_t(size));
    if (transmit(peer, std::span(handshake_).first(size + 3), false, now))
        peer.hello = now;
}

void Runtime::reject(Endpoint endpoint, std::uint64_t session, DisconnectReason reason) {
    auto *packet = transport_->send.writer();
    if (!packet)
        return;
    packet->endpoint = endpoint;
    Writer writer(std::span(packet->data).subspan(header_size));
    writer.put(std::uint8_t(Op::reject));
    writer.put(std::uint16_t(1));
    writer.put(std::uint8_t(reason));
    write_header(*packet, Header{.session = session, .sequence = 1, .payload_size = 4});
    transport_->publish();
}

void Runtime::hello(const Packet &packet, const Header &header, std::span<const std::byte> bytes) {
    Reader reader(bytes);
    if (!header.session || !schema_matches(reader) || !reader.done()) {
        reject(packet.endpoint, header.session, DisconnectReason::schema_mismatch);
        return;
    }
    if (endpoints_.contains(packet.endpoint.key()))
        return;
    auto slot = std::find(peers_.begin(), peers_.end(), nullptr);
    if (slot == peers_.end()) {
        reject(packet.endpoint, header.session, DisconnectReason::capacity);
        return;
    }
    auto peer = std::make_unique<Peer>();
    peer->endpoint = packet.endpoint;
    peer->session = header.session;
    peer->id = next_peer_++;
    peer->received = Clock::now();
    peer->channel.receive.accept(header.sequence);
    peer->outbox.init(limits_.max_entities, registry_->types.size());
    peer->visible.resize(std::size_t(limits_.max_entities) + 1);
    peer->entity = ecs::entity::create().add<Connection, Connected>()
        .set(PeerId{ peer->id }).set(Latency{}).set(Stats{}).id();
    peer->connected = true;
    auto *connection = peer.get();
    endpoints_.emplace(packet.endpoint.key(), slot - peers_.begin());
    *slot = std::move(peer);
    Writer writer(handshake_);
    writer.put(std::uint8_t(Op::welcome));
    writer.put(std::uint16_t(6 + 2 + registry_->types.size() * 16));
    writer.put(connection->id);
    writer.put(limits_.max_entities);
    auto offset = writer.size();
    auto size = schema_bytes(std::span(handshake_).subspan(offset));
    (void)transmit(*connection, std::span(handshake_).first(offset + size), true, Clock::now());
    update_scope(*connection);
    ecs::trigger<OnConnect>(ecs::entity(connection->entity));
}

static std::size_t frame_size(const Message &message) {
    if (message.op == Op::despawn)
        return 3 + 4;
    if (message.op == Op::spawn || message.op == Op::relations)
        return 3 + 8 + message.size;
    return 3 + 10 + message.size;
}

static void write_frame(Writer &writer, const Message &message) {
    writer.put(std::uint8_t(message.op));
    writer.put(std::uint16_t(frame_size(message) - 3));
    writer.put(message.entity);
    if (message.type)
        writer.put(message.type->slot);
    if (message.op != Op::despawn)
        writer.put(message.revision);
    writer.bytes({ message.value.data(), message.size });
}

bool Runtime::send_outbox(Peer &peer, Time now, std::uint32_t &budget) {
    auto *first = peer.outbox.front();
    if (!first)
        return false;
    bool reliable = first->reliable;
    std::array<std::byte, mtu - header_size> payload;
    Writer writer(payload);
    std::size_t count = 0;
    while (auto *message = peer.outbox.peek(count)) {
        if (message->reliable != reliable || frame_size(*message) > writer.remaining() ||
            header_size + writer.size() + frame_size(*message) > budget)
            break;
        write_frame(writer, *message);
        ++count;
    }
    if (!count || !transmit(peer, std::span(payload).first(writer.size()), reliable, now))
        return false;
    peer.outbox.consume(count);
    budget -= header_size + writer.size();
    return true;
}

void Runtime::send() {
    if (stopping_)
        return;
    auto now = Clock::now();
    for (auto &entry : peers_) {
        if (!entry)
            continue;
        auto &peer = *entry;
        if (!peer.closed && now - peer.received > std::chrono::duration<float>(limits_.timeout_seconds))
            disconnect(peer, DisconnectReason::timeout);
        if (server_ && peer.closed) {
            endpoints_.erase(peer.endpoint.key());
            entry.reset();
            continue;
        }
        if (!peer.connected) {
            if (!server_ && !peer.closed && peer.id == 0 && now - peer.hello >= std::chrono::milliseconds(250))
                send_hello(peer, now);
            continue;
        }
        std::uint32_t budget = limits_.bytes_per_tick;
        auto retry_after = std::chrono::duration<float, std::milli>(std::max(50.f, peer.channel.rtt_ms * 1.5f));
        for (auto &record : peer.channel.sent) {
            if (!record.reliable_sequence || now - record.sent < retry_after || record.packet.size > budget)
                continue;
            auto sequence = peer.channel.next_sequence;
            auto lookup = peer.channel.ack_lookup[sequence & 255];
            if (lookup && lookup != record.reliable_sequence)
                continue;
            auto *packet = transport_->send.writer();
            if (!packet)
                break;
            packet->copy_from(record.packet);
            Header header;
            read_header(packet->bytes(), header);
            peer.channel.ack_lookup[record.sequence & 255] = 0;
            peer.channel.ack_lookup[sequence & 255] = record.reliable_sequence;
            record.sequence = sequence;
            header.sequence = sequence;
            ++peer.channel.next_sequence;
            header.ack = peer.channel.receive.latest;
            header.ack_bits = peer.channel.receive.bits;
            write_header(*packet, header);
            budget -= packet->size;
            peer.stats.sent_bytes += packet->size;
            ++peer.stats.sent_packets;
            ++peer.stats.retransmissions;
            record.sent = now;
            record.retransmitted = true;
            peer.channel.ack_pending = false;
            peer.sent = now;
            transport_->publish();
        }
        while (!peer.outbox.empty() && send_outbox(peer, now, budget)) {}
        if (peer.channel.ack_pending || now - peer.sent >= std::chrono::milliseconds(500)) {
            const std::array<std::byte, 3> heartbeat{ std::byte(Op::heartbeat), std::byte(0), std::byte(0) };
            (void)transmit(peer, heartbeat, false, now);
        }
        ecs::entity(peer.entity).set(peer.stats).set(Latency{ peer.channel.rtt_ms });
    }
    transport_->flush();
}

} // namespace net::detail
