#pragma once

#include "match.h"
#include <stdexcept>

namespace game {
/** Server-only tuning; durations are in 60 Hz ticks, speeds in units per second. */
struct GameplayConfig {
    std::uint64_t seed = 0x6d6f7267616e65;
    std::uint64_t fire_interval = 10;
    float shot_speed = 480;
    float enemy_speed = 80;
    float enemy_shot_speed = 150;
    std::uint64_t wave_delay = 120;
    std::uint64_t wave_interval = 240;
    unsigned wave_size = 5;
    float spawn_spacing = 24;
    std::uint64_t enemy_fire_interval = 120;
    std::uint64_t invulnerability = 60;
    float despawn_margin = 48;
    float spawn_offset = 24;
};
/** Every authoritative gameplay module, sharing one GameplayConfig resource. */
struct ServerGameplay {
    static void import(const GameplayConfig &config = {}) {
        // Bodies spawn spawn_offset beyond the edge and must not be despawned there.
        if (!(config.despawn_margin > config.spawn_offset) || !(config.enemy_speed > 0))
            throw std::invalid_argument("invalid GameplayConfig");
        ecs::set_resource(config);
        ecs::import<ServerMovement>();
        ecs::import<ServerPlayers>();
        ecs::import<ServerCombat>();
        ecs::import<ServerWaves>();
        ecs::import<ServerMatch>();
    }
};
} // namespace game
