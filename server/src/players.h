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
/** Assigns each new ServerPlayer a free slot, spawn point, collider and health. */
struct ServerPlayers {
    static void import();
};
} // namespace game
