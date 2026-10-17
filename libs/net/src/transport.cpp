#include "transport.h"
#include <arpa/inet.h>
#include <cerrno>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

namespace net::detail {

sockaddr_in Endpoint::socket_address() const {
    sockaddr_in result{};
    result.sin_family = AF_INET;
    result.sin_addr.s_addr = address;
    result.sin_port = port;
    return result;
}

Endpoint Endpoint::parse(const char *address, std::uint16_t port) {
    Endpoint result{ .port = htons(port) };
    if (inet_pton(AF_INET, address, &result.address) != 1)
        throw std::invalid_argument("Network address must be an IPv4 literal");
    return result;
}

Transport::Transport(Endpoint bind_address) {
    socket_ = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (socket_ == -1)
        throw std::system_error(errno, std::generic_category(), "UDP socket");
    auto address = bind_address.socket_address();
    if (bind(socket_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == -1) {
        auto code = errno;
        close(socket_);
        throw std::system_error(code, std::generic_category(), "UDP bind");
    }
    wake_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wake_ == -1) {
        auto code = errno;
        close(socket_);
        throw std::system_error(code, std::generic_category(), "UDP wake event");
    }
    thread_ = std::jthread([this](std::stop_token stop) { run(stop); });
}

Transport::~Transport() {
    thread_.request_stop();
    wake();
    thread_.join();
    close(wake_);
    close(socket_);
}

void Transport::wake() {
    const std::uint64_t signal = 1;
    (void)write(wake_, &signal, sizeof(signal));
}

void Transport::flush() {
    if (send_pending_) {
        send_pending_ = false;
        wake();
    }
}

Endpoint Transport::local_endpoint() const {
    sockaddr_in address{};
    socklen_t size = sizeof(address);
    if (getsockname(socket_, reinterpret_cast<sockaddr *>(&address), &size) == -1)
        throw std::system_error(errno, std::generic_category(), "UDP local endpoint");
    return { address.sin_addr.s_addr, address.sin_port };
}

void Transport::run(std::stop_token stop) {
    Packet discard;
    pollfd descriptors[2] = { { socket_, POLLIN, 0 }, { wake_, POLLIN, 0 } };
    for (;;) {
        bool blocked = false;
        while (auto *packet = send.reader()) {
            auto address = packet->endpoint.socket_address();
            auto result = sendto(socket_, packet->data.data(), packet->size, 0,
                                 reinterpret_cast<sockaddr *>(&address), sizeof(address));
            if (result == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                blocked = true;
                break;
            }
            if (result == -1 && errno == EINTR)
                continue;
            if (result == -1)
                error.store(errno, std::memory_order_relaxed);
            send.consume();
        }
        if (stop.stop_requested())
            break;
        descriptors[0].events = POLLIN | (blocked ? POLLOUT : 0);
        if (poll(descriptors, 2, -1) == -1) {
            if (errno == EINTR)
                continue;
            error.store(errno, std::memory_order_relaxed);
            break;
        }
        if (descriptors[1].revents & POLLIN) {
            std::uint64_t signal;
            (void)read(wake_, &signal, sizeof(signal));
        }
        if (descriptors[0].revents & POLLIN) {
            for (unsigned batch = 0; batch < 256; ++batch) {
                auto *slot = receive.writer();
                auto &packet = slot ? *slot : discard;
                sockaddr_in address{};
                socklen_t size = sizeof(address);
                auto result = recvfrom(socket_, packet.data.data(), mtu, MSG_TRUNC,
                                       reinterpret_cast<sockaddr *>(&address), &size);
                if (result == -1) {
                    if (errno == EINTR)
                        continue;
                    if (errno != EAGAIN && errno != EWOULDBLOCK)
                        error.store(errno, std::memory_order_relaxed);
                    break;
                }
                if (!slot || result > std::ptrdiff_t(mtu)) {
                    dropped.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                packet.endpoint = { address.sin_addr.s_addr, address.sin_port };
                packet.size = result;
                receive.publish();
            }
        }
    }
}

} // namespace net::detail
