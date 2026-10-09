#include "protocol.h"
#include "runtime.h"
#include <cstdlib>
#include <cmath>
#include <iostream>

#define CHECK(expression) do { \
    if (!(expression)) { \
        std::cerr << "Check failed at line " << __LINE__ << ": " << #expression << '\n'; \
        std::abort(); \
    } \
} while (false)

using namespace net::detail;

struct ReflectedValue { reflected(uint8_t flags; float x; bool enabled;) };
struct PlainPosition { float x; float y; };
struct PositionCodec {
    static constexpr std::string_view schema_name = "test.position.centimeters.v1";
    static constexpr std::size_t wire_size = 4;
    static void encode(const PlainPosition &p, std::span<std::byte> bytes) {
        Writer writer(bytes);
        writer.put(std::bit_cast<std::uint16_t>(std::int16_t(std::lround(p.x * 100))));
        writer.put(std::bit_cast<std::uint16_t>(std::int16_t(std::lround(p.y * 100))));
    }
    static bool decode(PlainPosition &p, std::span<const std::byte> bytes) {
        Reader reader(bytes);
        p.x = std::bit_cast<std::int16_t>(reader.get<std::uint16_t>()) / 100.f;
        p.y = std::bit_cast<std::int16_t>(reader.get<std::uint16_t>()) / 100.f;
        return reader.done();
    }
};

int main() {
    ReceiveWindow window;
    window.accept(100);
    window.accept(102);
    CHECK(window.contains(100));
    CHECK(!window.contains(101));
    window.accept(101);
    CHECK(window.contains(101));
    window.accept(166);
    CHECK(window.contains(102));
    CHECK(!window.contains(165));
    window.accept(300);
    CHECK(window.bits == 0);
    CHECK(newer(1, UINT32_MAX));

    Packet packet;
    Header sent{ .session = 123, .sequence = 1, .ack = 17, .ack_bits = 9 };
    write_header(packet, sent);
    Header received;
    CHECK(read_header(packet.bytes(), received));
    CHECK(received.session == 123 && received.ack == 17 && received.ack_bits == 9);
    --packet.size;
    CHECK(!read_header(packet.bytes(), received));
    const std::array<std::byte, 4> truncated{ std::byte(Op::event), std::byte(4), std::byte(0), std::byte(0) };
    CHECK(!valid_frames(truncated));

    Descriptor type;
    type.local_index = 0;
    Outbox outbox;
    outbox.init(4, 1);
    Message value{ .entity = 0x10001, .revision = 1, .type = &type };
    CHECK(outbox.push(value));
    value.revision = 2;
    CHECK(outbox.push(value));
    CHECK(outbox.front()->revision == 2);
    outbox.pop();
    CHECK(outbox.empty());
    value.reliable = true;
    value.op = Op::remove;
    CHECK(outbox.push(value));
    value.op = Op::component;
    value.reliable = false;
    CHECK(outbox.push(value));
    CHECK(outbox.front()->reliable);

    Channel channel;
    auto now = Clock::now();
    channel.sent[1].sequence = 10;
    channel.sent[1].reliable_sequence = 1;
    channel.sent[1].sent = now - std::chrono::milliseconds(20);
    channel.ack_lookup[10] = 1;
    channel.in_flight = 1;
    channel.acknowledge(Header{.ack = 12, .ack_bits = 2}, now);
    CHECK(channel.in_flight == 0 && channel.rtt_ms > 0);
    channel.acknowledge(Header{.ack = 12, .ack_bits = 2}, now);
    CHECK(channel.in_flight == 0);

    ecs::init();
    net::replicate<ReflectedValue>();
    net::replicate<PlainPosition>().name("test.Position").codec<PositionCodec>();
    auto &r = registry();
    r.freeze();
    auto &reflected = *r.types[0];
    CHECK(reflected.wire_size == 6); // Padding never appears on the wire.
#ifndef NDEBUG
    CHECK(ecs_component_info(reflected.component)->mutation == EcsSetOnly);
#endif
    std::array<std::byte, 6> encoded;
    ReflectedValue original{9, 1.5f, true};
    reflected.encode(reflected, &original, encoded);
    ReflectedValue decoded{};
    CHECK(reflected.decode(reflected, &decoded, encoded));
    CHECK(decoded.flags == 9 && decoded.x == 1.5f && decoded.enabled);
    encoded.back() = std::byte(2);
    CHECK(!reflected.decode(reflected, &decoded, encoded));
    auto &quantized = *r.types[1];
    CHECK(quantized.type_id == hash("test.Position"));
    std::array<std::byte, 4> compact;
    PlainPosition position{1.23f, -4.56f};
    quantized.encode(quantized, &position, compact);
    PlainPosition restored{};
    CHECK(quantized.decode(quantized, &restored, compact));
    CHECK(std::abs(restored.x - position.x) < 0.001f && std::abs(restored.y - position.y) < 0.001f);
    ecs::fini();
    std::cout << "Protocol, outbox, reflection and custom codec checks passed\n";
}
