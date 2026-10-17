#include "internal.h"
#include <algorithm>
#include <cmath>

namespace physics::detail {

void Motion::import() {
    ecs::set_resource(SweepBounds{});
    ecs::resource<Pipeline>().simulated =
        ecs::system("physics.Simulate")
            .phase(ecs::resource<Phases>().simulate)
            .manual()
            .immediate()
            .no_defer()
            .after(ecs::resource<Pipeline>().synchronized)
            .each([](ecs::res<const NativeWorld> world,
                     ecs::res<SweepBounds> bounds,
                     ecs::res<Stats> stats) {
                *stats = {};
                bounds->displacement = {};
                b2World_Step(world->id, float(fixed_step), 4);
                const auto moves = b2World_GetBodyEvents(world->id);
                stats->bodies = moves.moveCount;
                for (int i = 0; i < moves.moveCount; ++i) {
                    const auto &move = moves.moveEvents[i];
                    auto body = entity_of(move.userData);
                    auto &native = body.get_mut<NativeBody>();
                    native.start = world->meters(native.current.x, native.current.y);
                    const b2Vec2 position{ float(move.transform.p.x), float(move.transform.p.y) };
                    native.displacement = b2Sub(position, native.start);
                    native.current = world->units(position);
                    const auto velocity = world->units(b2Body_GetLinearVelocity(native.body));
                    native.velocity = { velocity.x, velocity.y };
                    bounds->displacement.x =
                        std::max(bounds->displacement.x, std::abs(native.displacement.x));
                    bounds->displacement.y =
                        std::max(bounds->displacement.y, std::abs(native.displacement.y));
                }
            });
    ecs::resource<Pipeline>().published =
        ecs::system("physics.PublishMotion")
            .phase(ecs::resource<Phases>().finish)
            .manual()
            .immediate()
            .each([](ecs::entity body, const NativeBody &native, ecs::target<InWorld>) {
                body.set(native.current, native.velocity);
            });
}
} // namespace physics::detail
