#include "gameplay.h"
#include <shared/game.h>
#include <siecs.h>
#include <sinet/server.h>

int main() {
    // No workers: the whole simulation is a handful of tiny systems in one fixed step, where
    // dispatching to a pool costs more than it saves. Access declarations stay accurate, so
    // worker_threads can be raised once systems carry enough work to pay for it.
    ecs::init({ .target_fps = 60 });
    ecs::import<net::Server>(net::ServerConfig{
        .port = 4242,
        .tick_rate = 30,
        .max_clients = game::max_players,
    });
    game::register_network();

    ecs::import<game::ServerGameplay>();

    ecs::run();
}
