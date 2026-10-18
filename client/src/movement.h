#pragma once

#include <client/raylib/ray.h>
#include <shared/movement.h>

namespace game {

struct ClientMovementConfig {
    float interpolation_ticks = 6; // 100 ms at the simulation's 60 Hz.
    float max_extrapolation_ticks = 6;
    float max_backlog_ticks = 4;
    float correction_seconds = 0.09f;
    float snap_distance = 100;
};
/** Resource: newest server tick received, and the delayed tick presented this frame. Both
 * restart with each connection. */
struct ServerTimeline {
    std::uint64_t latest = 0;
    double cursor = 0;
};
struct ClientMovement {
    static void import(const ClientMovementConfig &config = {});
};

} // namespace game
