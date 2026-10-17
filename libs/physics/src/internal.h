#pragma once

#include <box2d/box2d.h>
#include <chrono>
#include <sibox2d/physics.h>
#include <vector>

namespace physics::detail {

struct ColliderChanged {};
struct NativeBody {
    b2BodyId body;
    b2ShapeId shape;
    Position2d current;
    Velocity2d velocity;
    b2Vec2 start, displacement, half;
};
struct NativeWorld {
    b2WorldId id;
    float units_per_meter;

    b2Vec2 meters(float x, float y) const { return { x / units_per_meter, y / units_per_meter }; }
    Position2d units(b2Vec2 v) const { return { v.x * units_per_meter, v.y * units_per_meter }; }
};
// Opaque Box2D user data carries the full ECS id, never a storage pointer.
inline void *user_data(ecs::entity entity) {
    return reinterpret_cast<void *>(std::uintptr_t(entity.id()));
}
inline ecs::entity entity_of(void *data) {
    return ecs::entity::from(std::uintptr_t(data));
}
struct RetiredBodies {
    std::vector<b2BodyId> ids;
};
struct SweepBounds {
    b2Vec2 displacement{};
};
struct First {};
struct Second {};
struct Touching {
    int sensors = 0;
    std::uint64_t swept_tick = 0;
    float fraction = 1;
    bool announced = false;
};
struct SensorChange {
    ecs::entity a, b;
    int change;
};
struct ContactBuffer {
    std::vector<Contact> events;
    std::vector<SensorChange> sensors;
    std::chrono::steady_clock::time_point sweep_started;
};
struct Pipeline {
    ecs_system_id_t published = 0;
};
struct Timing {
    double accumulator = 0;
};
struct Lifecycle {
    static void import();
};
struct Motion {
    static void import();
};
struct Contacts {
    static void import();
};

} // namespace physics::detail
