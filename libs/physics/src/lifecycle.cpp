#include "internal.h"

namespace physics::detail {
namespace {
void kill_contacts(ecs::entity body) {
    for (auto contact : body.sources<First>())
        contact.kill();
    for (auto contact : body.sources<Second>())
        contact.kill();
}
} // namespace

void Lifecycle::import() {
    ecs::set_resource(RetiredBodies{});
    ecs::component<Body>().with<Position2d, Velocity2d, Box, Filter, ExternalMotion2d>();
    ecs::component<Continuous>().with<Body>();
    ecs::component<NativeBody>(ecs::component_hooks<NativeBody>{
        .on_remove = [](ecs_entity_t, NativeBody &native) {
            ecs::resource<RetiredBodies>().ids.push_back(native.body);
        } });
    // Colliders change rarely, so a tag selects them; motion is diffed every step instead.
    ecs::observe<ecs::OnSet>().each_for<Box>([](ecs::entity body, const NativeBody &) {
        body.add<ColliderChanged>();
    });
    ecs::observe<ecs::OnSet>().each_for<Filter>([](ecs::entity body, const NativeBody &) {
        body.add<ColliderChanged>();
    });
    const auto phase = ecs::resource<Phases>().simulate;
    auto release = ecs::system("physics.ReleaseBodies")
                       .phase(phase)
                       .manual()
                       .exclude<Body>()
                       .each([](ecs::entity body, const NativeBody &) {
                           kill_contacts(body);
                           body.remove<NativeBody, ExternalMotion2d>();
                       });
    auto create = ecs::system("physics.CreateBodies")
                      .phase(phase)
                      .manual()
                      .require<Body>()
                      .exclude<NativeBody>()
                      .each([](ecs::entity body,
                               const Position2d &position,
                               const Velocity2d &velocity,
                               ecs::res<NativeWorld> world) {
                          auto definition = b2DefaultBodyDef();
                          definition.type = b2_dynamicBody;
                          definition.position = world->meters(position.x, position.y);
                          definition.linearVelocity = world->meters(velocity.x, velocity.y);
                          definition.motionLocks.angularZ = true;
                          definition.enableSleep = false;
                          definition.userData = user_data(body);
                          body.set(
                              NativeBody{ .body = b2CreateBody(world->id, &definition),
                                          .current = position,
                                          .velocity = velocity,
                                          .start = definition.position }
                          );
                          body.add<ColliderChanged>();
                      });
    // Consumes the NativeBody and ColliderChanged that CreateBodies adds.
    ecs::system("physics.UpdateColliders")
        .phase(phase)
        .manual()
        .after(create)
        .require<ColliderChanged>()
        .each([](ecs::entity body,
                 NativeBody &native,
                 const Box &box,
                 const Filter &filter,
                 ecs::res<NativeWorld> world) {
            if (B2_IS_NON_NULL(native.shape)) {
                kill_contacts(body);
                b2DestroyShape(native.shape, true);
            }
            native.half = world->meters(box.half_size.x, box.half_size.y);
            auto polygon = b2MakeBox(native.half.x, native.half.y);
            auto definition = b2DefaultShapeDef();
            definition.isSensor = true;
            definition.enableSensorEvents = true;
            definition.filter.categoryBits = filter.category;
            definition.filter.maskBits = filter.mask;
            definition.userData = user_data(body);
            native.shape = b2CreatePolygonShape(native.body, &definition, &polygon);
            body.remove<ColliderChanged>();
        });
    // NativeBody mirrors the last values pushed to Box2D, so any difference is an external write.
    ecs::system("physics.SyncBodies")
        .phase(phase)
        .manual()
        .each([](NativeBody &native,
                 const Position2d &position,
                 const Velocity2d &velocity,
                 ecs::res<NativeWorld> world) {
            if (position.x != native.current.x || position.y != native.current.y) {
                b2Body_SetTransform(
                    native.body,
                    world->meters(position.x, position.y),
                    b2Rot_identity
                );
                native.current = position;
            }
            if (velocity.x != native.velocity.x || velocity.y != native.velocity.y) {
                b2Body_SetLinearVelocity(native.body, world->meters(velocity.x, velocity.y));
                native.velocity = velocity;
            }
        });
    // NativeBody's remove hook fills RetiredBodies when ReleaseBodies' commands flush.
    ecs::system("physics.DestroyRetiredBodies")
        .phase(phase)
        .manual()
        .after(release)
        .each([](ecs::res<RetiredBodies> retired, ecs::res<NativeWorld>) {
            for (auto id : retired->ids)
                b2DestroyBody(id);
            retired->ids.clear();
        });
}
} // namespace physics::detail
