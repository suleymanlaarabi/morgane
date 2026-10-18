#include <game/components.h>
#include <game/detect.h>
#include <game/resolve.h>
#include <server.h>
#include <shared/game.h>
#include <sinet/server.h>

int main() {
    ecs::init({ .target_fps = 60, .worker_threads = ECS_WORKERS_AUTO });
    ecs::import<net::Server>(net::ServerConfig{
        .port = 4242,
        .tick_rate = 30,
        .max_clients = 128,
    });
    game::register_network();
    game::register_components();
    auto detection = game::install_detection();
    game::install_resolution(detection);

    ecs::observe<net::OnConnect>().each([](ecs::entity connection) {
        ecs::entity::create().add<net::Replicated>().relate<net::OwnedBy>(connection);
    });

    ecs::run();
}
