#pragma once

#include <client/scene/scene_manager.h>

namespace scenes {
struct Menu {};
/** Waiting for the server to accept the connection. */
struct Connecting {};
struct Game {
    int level = 1;
};
struct Pause {};
/** Volume, accessibility and key bindings, pushed over the menu or the pause screen. */
struct Options {};
using App = scene::Catalog<Menu, Connecting, Game, Pause, Options>;
using Commands = App::Commands;
/** Register the scenes and open the menu, or start joining the menu's address at once. */
void install(bool join = false);
} // namespace scenes
