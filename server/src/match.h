#pragma once

#include "waves.h"

namespace game {
/** Server-only tag: the player voted to play again. */
struct WantsRestart {};
/** Server-only: players present and votes, counted once per tick. */
struct Roster {
    unsigned players = 0;
    unsigned ready = 0;
};
/** Match phases: waiting for a player, playing, over once everyone is eliminated. When every
 * player votes to play again, the arena is cleared and all players respawn together. */
struct ServerMatch {
    static void import();
};
} // namespace game
