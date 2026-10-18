#pragma once

#include "ray.h"
#include <cstdint>
#include <span>

namespace ray {

/** Resource: master volume in [0, 1]. Setting it applies it at once. */
struct Volume {
    float master = 0.8f;
};

/** Module: opens the audio device and owns the sounds made by make_sound. Without an audio
 * device (headless machines) the game stays silent instead of failing. */
struct Audio {
    static void import(const Volume &volume = {});
};

/** Handle to a sound owned by the audio module. */
struct Sound {
    std::uint32_t id = UINT32_MAX;
};

/** Build a sound from mono samples in [-1, 1]. Call after importing ray::Audio. */
Sound make_sound(std::span<const float> samples, int sample_rate);

/** Play `sound` at `gain` times the master volume; overlapping plays of one sound restart it. */
void play(Sound sound, float gain = 1, float pitch = 1);

} // namespace ray
