#include "server.h"
#include "../../net/src/runtime.h"

namespace net {

void Server::import(const ServerConfig &config) {
    auto limits = config.limits;
    limits.tick_rate = config.tick_rate;
    ecs::set_resource(config);
    detail::install(std::make_shared<detail::Runtime>(
        true, limits, config.max_clients, detail::Endpoint::parse(config.bind_address.c_str(), config.port)),
        config.tick_rate);
}

} // namespace net
