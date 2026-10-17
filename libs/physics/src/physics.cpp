#include "internal.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace physics {

void advance(double seconds) {
    auto &timing = ecs::resource<detail::Timing>();
    auto &clock = ecs::resource<Clock>();
    const auto &phases = ecs::resource<Phases>();
    const auto &config = ecs::resource<WorldConfig>();
    const auto world = ecs::entity::create<World>();
    const auto available = timing.accumulator + seconds;
    timing.accumulator = std::min(available, fixed_step * config.max_catch_up_steps);
    clock.dropped_seconds += available - timing.accumulator;
    while (timing.accumulator + 1e-9 >= fixed_step) {
        const auto started = std::chrono::steady_clock::now();
        Step step{ clock.tick + 1, float(fixed_step) };
        ecs::trigger<BeforeStep>(world, &step);
        ecs::run_manual_phase(phases.input);
        ecs::run_manual_phase(phases.simulate);
        ++clock.tick;
        ecs::run_manual_phase(phases.finish);
        ecs::run_manual_phase(phases.output);
        ecs::trigger<AfterStep>(world, &step);
        timing.accumulator -= fixed_step;
        ecs::resource<Stats>().step_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
                .count();
    }
}

void World::import(const WorldConfig &config) {
    if (!std::isfinite(config.units_per_meter) || config.units_per_meter <= 0 ||
        !config.max_catch_up_steps || config.max_catch_up_steps > 60)
        throw std::invalid_argument("invalid physics WorldConfig");
    ecs::import<sispatial>();
    // The module entity carries BeforeStep and AfterStep.
    (void)ecs::entity::create<World>();
    ecs::set_resource(config);
    ecs::set_resource(Clock{});
    ecs::set_resource(Stats{});
    ecs::set_resource(detail::Timing{});
    ecs::set_resource(detail::Pipeline{});
    ecs::set_resource(
        Phases{ ecs::phase("physics.Input").after(EcsPreUpdate).before(EcsOnUpdate).id(),
                ecs::phase("physics.Simulate").after(EcsPreUpdate).before(EcsOnUpdate).id(),
                ecs::phase("physics.Finish").after(EcsPreUpdate).before(EcsOnUpdate).id(),
                ecs::phase("physics.Output").after(EcsPreUpdate).before(EcsOnUpdate).id() }
    );
    auto definition = b2DefaultWorldDef();
    definition.gravity = { 0, 0 };
    definition.enableSleep = false;
    definition.workerCount = 0;
    const auto native = b2CreateWorld(&definition);
    ecs::set_resource(detail::NativeWorld{ native, config.units_per_meter });
    ecs::at_fini([native] { b2DestroyWorld(native); });
    ecs::import<detail::Lifecycle>();
    ecs::import<detail::Motion>();
    ecs::import<detail::Contacts>();
    const auto driver = ecs::phase("physics.Frame").after(EcsPreUpdate).before(EcsOnUpdate).id();
    // Manual phases must be driven from the main thread, outside any defer scope.
    ecs::system("physics.FixedStep")
        .phase(driver)
        .immediate()
        .no_defer()
        .each([](ecs::res<const DeltaTime> dt) { advance(dt->value); });
}
} // namespace physics
