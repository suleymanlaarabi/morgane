#pragma once

#include "scene_manager.h"
#include <client/raylib/animation.h>

namespace scenes {
inline constexpr ray::Animation ShipIdle = {
    .frame = { 0, 0, 33, 17 },
    .frames = 5,
    .fps = 8,
    .playback = ray::Playback::ping_pong,
};
struct Menu {};
struct Game {
    int level = 1;
};
struct Pause {};
using App = scene::Catalog<Menu, Game, Pause>;
using Commands = App::Commands;
void install();
} // namespace scenes
