#include "client.h"
#include "../../net/src/runtime.h"

namespace net {

void Client::import(const ClientConfig &config) {
    ecs::set_resource(config);
    detail::install(std::make_shared<detail::Runtime>(
        false, config.limits, 1, detail::Endpoint::parse("0.0.0.0", 0),
        detail::Endpoint::parse(config.address.c_str(), config.port)), config.limits.tick_rate);
}

} // namespace net
