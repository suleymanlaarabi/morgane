#include "client.h"
#include "../../libs/net/src/runtime.h"

namespace net {

void Client::import(const ClientConfig &config) {
    ecs::set_resource(config);
    ecs::set_resource(LocalPeer{});
    (void)ecs::component<Controlled>();
    detail::install(std::make_shared<detail::Runtime>(
        false, config.limits, 1, detail::Endpoint::parse("0.0.0.0", 0)), config.limits.tick_rate);
    if (config.connect)
        connect(config.address, config.port);
}

void connect(std::string_view address, std::uint16_t port) {
    const std::string host(address);
    ecs::resource<detail::State>().runtime->request_open(detail::Endpoint::resolve(host.c_str(), port));
}

void disconnect() { ecs::resource<detail::State>().runtime->request_close(); }

} // namespace net
