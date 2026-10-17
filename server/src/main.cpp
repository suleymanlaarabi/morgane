#include "movement.h"
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

    ecs::import<game::ServerMovement>();

    ecs::run();
}
