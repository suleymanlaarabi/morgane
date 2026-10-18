#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <type_traits>

namespace net::detail {

inline constexpr std::size_t mtu = 1200;
inline constexpr std::size_t max_value_size = 256;
inline constexpr std::size_t max_types = 64;

class Writer {
    std::span<std::byte> buffer_;
    std::size_t cursor_ = 0;

  public:
    explicit Writer(std::span<std::byte> buffer) : buffer_(buffer) {}
    template <typename T> void put(T value) {
        static_assert(std::is_unsigned_v<T>);
        for (std::size_t i = 0; i < sizeof(T); ++i)
            buffer_[cursor_++] = std::byte(value >> (i * 8));
    }
    void bytes(std::span<const std::byte> value) {
        std::memcpy(buffer_.data() + cursor_, value.data(), value.size());
        cursor_ += value.size();
    }
    std::size_t size() const { return cursor_; }
    std::size_t remaining() const { return buffer_.size() - cursor_; }
};

class Reader {
    std::span<const std::byte> buffer_;
    std::size_t cursor_ = 0;
    bool valid_ = true;

  public:
    explicit Reader(std::span<const std::byte> buffer) : buffer_(buffer) {}
    template <typename T> T get() {
        static_assert(std::is_unsigned_v<T>);
        auto value = bytes(sizeof(T));
        T result = 0;
        for (std::size_t i = 0; i < value.size(); ++i)
            result |= T(std::to_integer<std::uint8_t>(value[i])) << (i * 8);
        return result;
    }
    std::span<const std::byte> bytes(std::size_t count) {
        if (count > remaining()) {
            valid_ = false;
            return {};
        }
        auto value = buffer_.subspan(cursor_, count);
        cursor_ += count;
        return value;
    }
    std::size_t remaining() const { return buffer_.size() - cursor_; }
    bool valid() const { return valid_; }
    bool done() const { return valid_ && remaining() == 0; }
};

constexpr std::uint64_t hash(std::string_view value, std::uint64_t seed = 14695981039346656037ull) {
    for (unsigned char c : value)
        seed = (seed ^ c) * 1099511628211ull;
    return seed;
}

constexpr bool newer(std::uint32_t value, std::uint32_t previous) {
    return std::bit_cast<std::int32_t>(value - previous) > 0;
}

} // namespace net::detail
