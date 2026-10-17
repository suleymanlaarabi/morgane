#pragma once

#include <sinet/net.h>

namespace net {

struct ClientConfig {
    std::string address = "127.0.0.1";
    std::uint16_t port = 4242;
    Limits limits{};
};

struct Client {
    static void import(const ClientConfig &config);
};

} // namespace net
