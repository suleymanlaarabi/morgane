#include "gameplay.h"
#include <charconv>
#include <cstdio>
#include <shared/game.h>
#include <shared/scene.h>
#include <siecs.h>
#include <sinet/server.h>
#include <string_view>

/** Usage: r-type_server [port], 4242 by default. */
int main(int argc, char **argv) {
    std::uint16_t port = 4242;
    if (argc > 1) {
        const std::string_view text = argv[1];
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), port);
        if (error != std::errc{} || end != text.data() + text.size() || !port) {
            std::fprintf(stderr, "usage: %s [port]\n", argv[0]);
            return 2;
        }
    }
    // No workers: the whole simulation is a handful of tiny systems in one fixed step, where
    // dispatching to a pool costs more than it saves. Access declarations stay accurate, so
    // worker_threads can be raised once systems carry enough work to pay for it.
    ecs::init({ .target_fps = 60 });
    ecs::import<net::Server>(net::ServerConfig{
        .port = port,
        .tick_rate = 30,
        .max_clients = game::max_players,
        // A crashed client is reported to the others after this long without packets.
        .limits = { .timeout_seconds = 5 },
    });
    game::register_network();

    ecs::import<game::ServerGameplay>();
    if (!game::load_level(false)) {
        std::fprintf(stderr, "failed to load scene: %s\n", ecs_scene_error() ? ecs_scene_error() : "unknown");
        return 1;
    }

    std::printf("r-type_server listening on UDP port %u\n", unsigned(port));
    std::fflush(stdout);
    ecs::run();
}
