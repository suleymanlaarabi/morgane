#include "internal.h"
#include <algorithm>
#include <chrono>

namespace physics::detail {
namespace {

// Pair storage and endpoint lifetime belong to SIECS, through indexed relations.
Touching &contact_pair(ecs::entity a, ecs::entity b) {
    if (a.id() > b.id())
        std::swap(a, b);
    for (auto contact : a.sources<First>())
        if (contact.target<Second>().id() == b.id())
            return contact.get_mut<Touching>();
    return ecs::entity::create()
        .relate<First>(a)
        .relate<Second>(b)
        .add<Touching>()
        .get_mut<Touching>();
}

b2ShapeProxy proxy(b2Vec2 half) {
    const b2Vec2 points[] = { { -half.x, -half.y },
                              { half.x, -half.y },
                              { half.x, half.y },
                              { -half.x, half.y } };
    return b2MakeProxy(points, 4, 0);
}
struct Cast {
    ecs::entity projectile;
    const NativeBody &motion;
    ContactBuffer &buffer;
    Stats &stats;
    std::uint64_t tick;
};
// Box2D's required C callback is the only callback adapter outside SIECS.
bool cast_candidate(b2ShapeId shape, void *context) {
    auto &cast = *static_cast<Cast *>(context);
    auto target = entity_of(b2Shape_GetUserData(shape));
    if (target.id() == cast.projectile.id())
        return true;
    const auto &motion = target.get<NativeBody>();
    ++cast.stats.casts;
    b2DistanceInput distance{};
    distance.proxyA = proxy(motion.half);
    distance.proxyB = proxy(cast.motion.half);
    distance.transform = { b2Sub(cast.motion.start, motion.start), b2Rot_identity };
    distance.useRadii = true;
    b2SimplexCache cache{};
    if (b2ShapeDistance(&distance, &cache, nullptr, 0).distance <= 0.00001f) {
        cast.buffer.events.push_back({ cast.projectile, target, cast.tick, 0 });
        return true;
    }
    b2ShapeCastPairInput input{};
    input.proxyA = distance.proxyA;
    input.proxyB = distance.proxyB;
    input.transform = distance.transform;
    input.translationB = b2Sub(cast.motion.displacement, motion.displacement);
    input.maxFraction = 1;
    const auto hit = b2ShapeCast(&input);
    if (hit.hit)
        cast.buffer.events.push_back({ cast.projectile, target, cast.tick, hit.fraction });
    return true;
}
} // namespace

void Contacts::import() {
    ecs::relation<First>({ .storage = EcsRelationDense, .on_delete_target = EcsDeleteSources });
    ecs::relation<Second>({ .storage = EcsRelationDense, .on_delete_target = EcsDeleteSources });
    ecs::set_resource(ContactBuffer{});
    const auto phase = ecs::resource<Phases>().simulate;
    auto sensors = ecs::system("physics.CopySensorEvents")
                       .phase(phase)
                       .manual()
                       .immediate()
                       .no_defer()
                       .after(ecs::resource<Pipeline>().simulated)
                       .each([](ecs::res<const NativeWorld> world, ecs::res<ContactBuffer> buffer) {
                           buffer->events.clear();
                           buffer->sensors.clear();
                           const auto push = [&](b2ShapeId sensor, b2ShapeId visitor, int change) {
                               buffer->sensors.push_back({ entity_of(b2Shape_GetUserData(sensor)),
                                                           entity_of(b2Shape_GetUserData(visitor)),
                                                           change });
                           };
                           const auto events = b2World_GetSensorEvents(world->id);
                           for (int i = 0; i < events.endCount; ++i) {
                               const auto &event = events.endEvents[i];
                               // Box2D explicitly permits destroyed shape ids in end events.
                               if (b2Shape_IsValid(event.sensorShapeId) &&
                                   b2Shape_IsValid(event.visitorShapeId))
                                   push(event.sensorShapeId, event.visitorShapeId, -1);
                           }
                           for (int i = 0; i < events.beginCount; ++i)
                               push(events.beginEvents[i].sensorShapeId,
                                    events.beginEvents[i].visitorShapeId,
                                    1);
                       });
    auto casts = ecs::system("physics.SweepProjectiles")
                     .phase(phase)
                     .manual()
                     .immediate()
                     .after(sensors)
                     .require<Continuous>()
                     .each([](ecs::entity body,
                              const NativeBody &motion,
                              const Filter &filter,
                              ecs::target<InWorld>,
                              ecs::res<const NativeWorld> world,
                              ecs::res<const SweepBounds> bounds,
                              ecs::res<ContactBuffer> buffer,
                              ecs::res<Stats> stats,
                              ecs::res<const Clock> clock) {
                         const auto started = std::chrono::steady_clock::now();
                         const auto end = b2Add(motion.start, motion.displacement);
                         const auto extent = b2Add(motion.half, bounds->displacement);
                         const b2AABB area{ { std::min(motion.start.x, end.x) - extent.x,
                                              std::min(motion.start.y, end.y) - extent.y },
                                            { std::max(motion.start.x, end.x) + extent.x,
                                              std::max(motion.start.y, end.y) + extent.y } };
                         Cast cast{ body, motion, *buffer, *stats, clock->tick + 1 };
                         b2World_OverlapAABB(
                             world->id,
                             b2Pos_zero,
                             area,
                             { filter.category, filter.mask },
                             cast_candidate,
                             &cast
                         );
                         stats->cast_ms += std::chrono::duration<double, std::milli>(
                                               std::chrono::steady_clock::now() - started
                         )
                                               .count();
                     });
    ecs::system("physics.ResolveContactPairs")
        .phase(phase)
        .manual()
        .immediate()
        .no_defer()
        .after(casts)
        .each([](ecs::res<ContactBuffer> buffer) {
            for (const auto &sensor : buffer->sensors)
                contact_pair(sensor.a, sensor.b).sensors += sensor.change;
            for (const auto &event : buffer->events) {
                auto &pair = contact_pair(event.a, event.b);
                pair.fraction = pair.swept_tick == event.tick
                                    ? std::min(pair.fraction, event.fraction)
                                    : event.fraction;
                pair.swept_tick = event.tick;
            }
            buffer->events.clear();
        });
    auto collect =
        ecs::system("physics.BeginContacts")
            .phase(ecs::resource<Phases>().finish)
            .manual()
            .immediate()
            .after(ecs::resource<Pipeline>().published)
            .each([](ecs::entity contact,
                     Touching &pair,
                     ecs::target<First> a,
                     ecs::target<Second> b,
                     ecs::res<const Clock> clock,
                     ecs::res<ContactBuffer> buffer) {
                if (pair.sensors <= 0 && pair.swept_tick != clock->tick) {
                    contact.kill();
                } else if (!pair.announced) {
                    pair.announced = true;
                    buffer->events.push_back(
                        { a, b, clock->tick, pair.swept_tick == clock->tick ? pair.fraction : 1 }
                    );
                }
            });
    // The query iteration defers deletions requested by gameplay observers. SIECS
    // keeps every endpoint valid until this whole batch of contacts is delivered.
    ecs::system("physics.EmitContacts")
        .phase(ecs::resource<Phases>().finish)
        .manual()
        .immediate()
        .after(collect)
        .require<Root>()
        .each([](ecs::entity,
                 ecs::target<InWorld>,
                 ecs::res<ContactBuffer> buffer,
                 ecs::res<Stats> stats) {
            auto &events = buffer->events;
            std::sort(events.begin(), events.end(), [](const Contact &a, const Contact &b) {
                if (a.fraction != b.fraction)
                    return a.fraction < b.fraction;
                if (a.a.id() != b.a.id())
                    return a.a.id() < b.a.id();
                return a.b.id() < b.b.id();
            });
            stats->contacts = events.size();
            for (const auto &event : events)
                ecs::trigger<OnContact>(event.a, &event);
            events.clear();
        });
}
} // namespace physics::detail
