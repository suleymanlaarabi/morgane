#pragma once

#include "ray.h"
#include <cstdint>

namespace ray {

/** Module: advances sprite animations. Import alongside ray::Window. */
struct Animations {
    static void import();
};

enum class Playback : std::uint8_t { loop, once, ping_pong };

/** Clip: `frames` frames of `frame` size, laid out left to right (wrapping after `columns`). */
struct Animation {
    Rectangle frame{};
    std::uint16_t frames = 1;
    float fps = 10;
    Playback playback = Playback::loop;
    std::uint16_t columns = 0; // 0 = single row
};

/** Playback state, added automatically with Animation. Zero = first frame, playing. */
struct Animator {
    float time;
    std::uint16_t frame;
    bool paused;
};

/** Triggered on the entity when a `once` clip reaches its last frame. */
struct AnimationFinished {};

/** Switch clip; restarts only when it differs from the current one. */
void play(ecs::entity entity, const Animation &clip);

} // namespace ray
