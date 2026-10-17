#pragma once

#include "scene_manager.h"

namespace scenes {
struct Menu {};
struct Game {
    int level = 1;
};
struct Pause {};
using App = scene::Catalog<Menu, Game, Pause>;
using Commands = App::Commands;
void install();
} // namespace scenes
