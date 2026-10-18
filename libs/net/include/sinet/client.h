#pragma once

#include <sinet/net.h>

namespace net {

struct ClientConfig {
    std::string address = "127.0.0.1";
    std::uint16_t port = 4242;
    Limits limits{};
    bool connect = true; // connect to address:port on import; otherwise wait for net::connect
};

struct Client {
    static void import(const ClientConfig &config);
};

/** Connect to `address` (IPv4 literal or host name) on `port`, closing any open connection
 * first. Applied at the next NetReceive; OnConnect or OnDisconnect reports the outcome.
 * Throws std::invalid_argument when the address does not resolve. */
void connect(std::string_view address, std::uint16_t port);
/** Close the connection at the next NetReceive. OnDisconnect fires with DisconnectReason::local
 * and every replicated entity is despawned, as on any client disconnect. */
void disconnect();

} // namespace net
