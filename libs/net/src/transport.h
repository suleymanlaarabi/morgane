#pragma once

#include <sinet/wire.h>
#include <atomic>
#include <netinet/in.h>
#include <stop_token>
#include <thread>

namespace net::detail {

struct Endpoint {
    std::uint32_t address = 0;
    std::uint16_t port = 0;
    bool operator==(const Endpoint &) const = default;
    std::uint64_t key() const { return (std::uint64_t(address) << 16) | port; }
    sockaddr_in socket_address() const;
    static Endpoint parse(const char *address, std::uint16_t port);
};

struct Packet {
    Endpoint endpoint;
    std::uint16_t size = 0;
    std::array<std::byte, mtu> data{};
    std::span<const std::byte> bytes() const { return { data.data(), size }; }
    void copy_from(const Packet &source) {
        endpoint = source.endpoint;
        size = source.size;
        std::memcpy(data.data(), source.data.data(), size);
    }
};

template <typename T, std::size_t Capacity> class SpscQueue {
    static_assert(std::has_single_bit(Capacity));
    std::array<T, Capacity> slots_;
    alignas(64) std::atomic<std::uint32_t> read_{0};
    alignas(64) std::atomic<std::uint32_t> write_{0};

  public:
    T *writer() {
        auto write = write_.load(std::memory_order_relaxed);
        if (write - read_.load(std::memory_order_acquire) == Capacity)
            return nullptr;
        return &slots_[write & (Capacity - 1)];
    }
    void publish() { write_.fetch_add(1, std::memory_order_release); }
    T *reader() {
        auto read = read_.load(std::memory_order_relaxed);
        if (read == write_.load(std::memory_order_acquire))
            return nullptr;
        return &slots_[read & (Capacity - 1)];
    }
    void consume() { read_.fetch_add(1, std::memory_order_release); }
};

class Transport {
    int socket_;
    int wake_;
    std::jthread thread_;
    bool send_pending_ = false;
    void run(std::stop_token stop);

  public:
    SpscQueue<Packet, 4096> receive;
    SpscQueue<Packet, 4096> send;
    std::atomic<std::uint64_t> dropped{0};
    std::atomic<int> error{0};
    explicit Transport(Endpoint bind);
    ~Transport();
    Transport(const Transport &) = delete;
    Transport &operator=(const Transport &) = delete;
    void wake();
    void publish() { send.publish(); send_pending_ = true; }
    void flush();
    Endpoint local_endpoint() const;
};

} // namespace net::detail
