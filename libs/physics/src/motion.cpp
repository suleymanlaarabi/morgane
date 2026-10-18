#include "internal.h"
#include <algorithm>
#include <cmath>

namespace physics::detail {

void Motion::import() {
    ecs::set_resource(SweepBounds{});
    const auto &phases = ecs::resource<Phases>();
    ecs::system("physics.Simulate")
        .phase(phases.simulate)
        .manual()
        .each([](ecs::res<NativeWorld> world, ecs::res<SweepBounds> bounds, ecs::res<Stats> stats) {
            *stats = {};
            bounds->displacement = {};
            b2World_Step(world->id, float(fixed_step), 4);
        });
    // Bodies never sleep, so every body is read back; sweeps need this step's displacement.
    ecs::system("physics.ReadMotion")
        .phase(phases.simulate)
        .manual()
        .each([](NativeBody &native,
                 ecs::res<const NativeWorld> world,
                 ecs::res<SweepBounds> bounds,
                 ecs::res<Stats> stats) {
            ++stats->bodies;
            native.start = world->meters(native.current.x, native.current.y);
            const auto position = b2Body_GetPosition(native.body);
            native.displacement = b2Sub(position, native.start);
            native.current = world->units(position);
            const auto velocity = world->units(b2Body_GetLinearVelocity(native.body));
            native.velocity = { velocity.x, velocity.y };
            bounds->displacement.x =
                std::max(bounds->displacement.x, std::abs(native.displacement.x));
            bounds->displacement.y =
                std::max(bounds->displacement.y, std::abs(native.displacement.y));
        });
    // Written in place: no command or observer per body, and SyncBodies then sees no change.
    ecs::resource<Pipeline>().published =
        ecs::system("physics.PublishMotion")
            .phase(phases.finish)
            .manual()
            .each([](const NativeBody &native, Position2d &position, Velocity2d &velocity) {
                position = native.current;
                velocity = native.velocity;
            });
}
} // namespace physics::detail
