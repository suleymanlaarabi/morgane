#pragma once

#include <sinet/net.h>

namespace net {

struct ServerConfig {
    std::uint16_t port = 4242;
    std::uint16_t tick_rate = 30;
    std::uint16_t max_clients = 128;
    std::string bind_address = "0.0.0.0";
    Limits limits{};
};

struct Server {
    static void import(const ServerConfig &config);
};

} // namespace net
