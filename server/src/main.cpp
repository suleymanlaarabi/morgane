#include "gameplay.h"
#include <shared/game.h>
#include <siecs.h>
#include <sinet/server.h>

int main() {
    ecs::init({ .target_fps = 60, .worker_threads = ECS_WORKERS_AUTO });
    ecs::import<net::Server>(net::ServerConfig{
        .port = 4242,
        .tick_rate = 30,
        .max_clients = game::max_players,
    });
    game::register_network();

    ecs::import<game::ServerGameplay>();

    ecs::run();
}
