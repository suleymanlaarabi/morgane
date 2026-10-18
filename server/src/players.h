#pragma once

#include "movement.h"

namespace game {
/** Server-only: the first tick at which the player may fire again. */
struct Weapon {
    std::uint64_t ready_tick = 0;
};
/** Server-only: hits are ignored before this tick. */
struct Invulnerable {
    std::uint64_t until_tick = 0;
};
/** Server-only resource: the replicated entity that carries Match. */
struct Arena {
    ecs::entity match;
};

inline Position2d spawn_point(std::uint8_t slot) {
    return { -play_half_width + 48, -60 + float(slot) * 40 };
}

/** Creates the match entity, then gives each new ServerPlayer a free slot, spawn point, collider
 * and health. Players joining a finished match wait for the next one, eliminated. */
struct ServerPlayers {
    static void import();
};
} // namespace game
