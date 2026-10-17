#include "protocol.h"
#include <catch2/catch_test_macros.hpp>
#include <sinet/wire.h>

using namespace net::detail;

TEST_CASE("wire values round-trip in little endian") {
    std::array<std::byte, 16> buffer{};
    Writer writer(buffer);
    writer.put(std::uint16_t(0x1234));
    writer.put(std::uint64_t(0x0102030405060708));
    CHECK(writer.size() == 10);
    CHECK(buffer[0] == std::byte(0x34));

    Reader reader{ std::span(buffer).first(writer.size()) };
    CHECK(reader.get<std::uint16_t>() == 0x1234);
    CHECK(reader.get<std::uint64_t>() == 0x0102030405060708);
    CHECK(reader.done());
}

TEST_CASE("reading past the end invalidates the reader") {
    std::array<std::byte, 2> buffer{};
    Reader reader(buffer);
    CHECK(reader.get<std::uint32_t>() == 0);
    CHECK_FALSE(reader.valid());
}

TEST_CASE("sequence comparison survives wrap-around") {
    CHECK(newer(2, 1));
    CHECK_FALSE(newer(1, 2));
    CHECK(newer(0, UINT32_MAX));
    CHECK_FALSE(newer(5, 5));
}

TEST_CASE("type hashes are stable FNV-1a") {
    STATIC_REQUIRE(hash("") == 14695981039346656037ull);
    STATIC_REQUIRE(hash("a") == 0xaf63dc4c8601ec8cull);
}

TEST_CASE("headers round-trip and reject malformed packets") {
    const Header sent{ .flags = reliable_flag,
                       .session = 42,
                       .sequence = 7,
                       .ack = 6,
                       .ack_bits = 0b101,
                       .reliable_sequence = 3,
                       .payload_size = 0 };
    Packet packet;
    write_header(packet, sent);

    Header received;
    REQUIRE(read_header(packet.bytes(), received));
    CHECK(received.session == 42);
    CHECK(received.sequence == 7);
    CHECK(received.ack_bits == 0b101);
    CHECK(received.reliable_sequence == 3);

    SECTION("wrong magic") {
        packet.data[0] ^= std::byte(1);
        CHECK_FALSE(read_header(packet.bytes(), received));
    }
    SECTION("truncated") {
        CHECK_FALSE(read_header(packet.bytes().first(header_size - 1), received));
    }
    SECTION("reliable flag without a reliable sequence") {
        write_header(packet, Header{ .flags = reliable_flag, .sequence = 1 });
        CHECK_FALSE(read_header(packet.bytes(), received));
    }
}

TEST_CASE("frames must fit in the payload") {
    std::array<std::byte, 8> buffer{};
    Writer writer(buffer);
    writer.put(std::uint8_t(Op::event));
    writer.put(std::uint16_t(2));
    writer.put(std::uint16_t(0));
    CHECK(valid_frames(std::span(buffer).first(writer.size())));
    CHECK_FALSE(valid_frames(std::span(buffer).first(writer.size() - 1)));

    buffer[0] = std::byte(0xff);
    CHECK_FALSE(valid_frames(std::span(buffer).first(writer.size())));
}

TEST_CASE("the receive window tracks the last 64 sequences") {
    ReceiveWindow window;
    CHECK_FALSE(window.contains(1));
    window.accept(1);
    window.accept(3);
    CHECK(window.contains(1));
    CHECK_FALSE(window.contains(2));
    CHECK(window.contains(3));
    CHECK_FALSE(window.contains(4));
}
