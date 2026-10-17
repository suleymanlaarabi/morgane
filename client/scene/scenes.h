#pragma once

#include "scene_manager.h"

namespace scenes {
    struct Menu {};
    struct Game {};

    scene::ScenesConfig config();
    void register_all(); // call after import of scene::Scenes
}
