#pragma once

#include <cstdint>
#include <siecs_spatial.h>

namespace physics {

inline constexpr double fixed_step = 1.0 / 60.0;

/** Owns motion. All boxes are sensors: contacts never push or bounce. */
struct Body {};
/** Swept detection, including targets moving across the path between steps. */
struct Continuous {};
struct Box {
    Position2d half_size{ 12, 8 };
};
struct Filter {
    std::uint64_t category = 1;
    std::uint64_t mask = UINT64_MAX;
};
struct WorldConfig {
    float units_per_meter = 100;
    unsigned max_catch_up_steps = 8;
};
/** Register manual systems in input/output to run once per fixed step. */
struct Phases {
    ecs_phase_t input;
    ecs_phase_t simulate;
    ecs_phase_t finish;
    ecs_phase_t output;
};
/** Resource, updated only at completed step boundaries. */
struct Clock {
    std::uint64_t tick = 0;
    double dropped_seconds = 0;
};
struct Stats {
    unsigned bodies = 0;
    unsigned contacts = 0;
    unsigned casts = 0;
    double step_ms = 0;
    double cast_ms = 0;
};
struct Step {
    std::uint64_t tick;
    float delta_time;
};
/** Native SIECS events on the module entity, with Step payload. */
struct BeforeStep {};
struct AfterStep {};
/** One event per unordered pair when contact begins, delivered on a.
 * Fraction orders swept impacts within this step. Observers may destroy either entity.
 */
struct Contact {
    ecs::entity a;
    ecs::entity b;
    std::uint64_t tick;
    float fraction;
};
struct OnContact {};
struct World {
    static void import(const WorldConfig &config = {});
};

/** Advance a headless world, or use the automatic ECS phase before OnUpdate.
 * Only call on the main thread, outside ECS deferral and outside a step callback.
 */
void advance(double seconds);

} // namespace physics
