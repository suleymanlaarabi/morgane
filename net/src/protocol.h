#pragma once

#include "transport.h"
#include <chrono>

namespace net::detail {

using Clock = std::chrono::steady_clock;
using Time = Clock::time_point;
inline constexpr std::uint32_t magic = 0x53494e45;
inline constexpr std::uint16_t protocol_version = 1;
inline constexpr std::size_t header_size = 38;
inline constexpr std::uint16_t reliable_flag = 1;
inline constexpr std::size_t reliable_window = 64;

enum class Op : std::uint8_t {
    hello, welcome, reject, heartbeat, disconnect, spawn, despawn, relations,
    component, remove, event
};

struct Header {
    std::uint16_t flags = 0;
    std::uint64_t session = 0;
    std::uint32_t sequence = 0;
    std::uint32_t ack = 0;
    std::uint64_t ack_bits = 0;
    std::uint32_t reliable_sequence = 0;
    std::uint16_t payload_size = 0;
};

bool read_header(std::span<const std::byte> bytes, Header &header);
void write_header(Packet &packet, const Header &header);
bool valid_frames(std::span<const std::byte> bytes);

struct ReceiveWindow {
    std::uint32_t latest = 0;
    std::uint64_t bits = 0;
    bool contains(std::uint32_t sequence) const;
    void accept(std::uint32_t sequence);
};

struct SentPacket {
    Packet packet;
    std::uint32_t sequence = 0;
    std::uint32_t reliable_sequence = 0;
    Time sent{};
    bool retransmitted = false;
};
struct BufferedPacket {
    Packet packet;
    std::uint32_t reliable_sequence = 0;
};

struct Channel {
    ReceiveWindow receive;
    std::uint32_t next_sequence = 1;
    std::uint32_t next_reliable = 1;
    std::uint32_t expected_reliable = 1;
    std::uint32_t in_flight = 0;
    std::array<SentPacket, reliable_window> sent;
    std::array<BufferedPacket, reliable_window> buffered;
    std::array<std::uint32_t, 256> ack_lookup{};
    bool ack_pending = false;
    float rtt_ms = 100;
    void acknowledge(const Header &header, Time now);
};

} // namespace net::detail
