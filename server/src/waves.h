#pragma once

#include "combat.h"
#include <array>
#include <random>

namespace game {
/** Server-only schedule of the next enemy spawn, in absolute ticks. */
struct WaveDirector {
    std::uint64_t next_spawn = 0;
    std::uint32_t remaining = 0;
    std::uint32_t wave = 0;
};
/** Server-only gameplay randomness, seeded from GameplayConfig. */
struct Rng {
    std::mt19937 engine;
};
/** Server-only: positions of the players still in play, gathered once per tick. */
struct AlivePlayers {
    std::array<Position2d, max_players> positions{};
    unsigned count = 0;
};
/** Server-only: the first tick at which the enemy may fire again. */
struct EnemyGun {
    std::uint64_t next_tick = 0;
};
/** Enemy waves while anyone is alive, and enemy fire at the nearest player. */
struct ServerWaves {
    static void import();
};
} // namespace game
