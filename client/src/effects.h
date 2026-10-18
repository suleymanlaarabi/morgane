#pragma once

#include <client/raylib/audio.h>

namespace game {

/** Resource: the game's sound effects, synthesized at startup. */
struct Sfx {
    ray::Sound shot, explosion, hit, eliminated, game_over;
};

/** Module: hit flashes, explosions and sound effects. Every sound has a visual counterpart:
 * hits blink the ship and lower its HUD health, destructions leave an explosion. Import after
 * ray::Audio, ray::Animations, game::ClientMovement and game::PreferencesModule. */
struct Effects {
    static void import();
};

} // namespace game
