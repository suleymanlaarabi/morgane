#pragma once

#include <shared/game.h>

namespace game {

/** Resource: the replicated Match and the local player's part in it, for the UI. */
struct MatchView {
    Match match{};
    bool eliminated = false; // the local player watches the others
    bool voted = false;      // the local player asked to play again
};

/** Module: sprites, callsigns and HUD lines for replicated players, shots and enemies, and the
 * MatchView resource. Import after game::register_network() and game::Effects. */
struct Visuals {
    static void import();
};

} // namespace game
