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
    ecs::component<Body>()
        .with<Position2d, Velocity2d, Box, Filter, ExternalMotion2d>()
        .with_relation<InWorld>(ecs::entity::create<World>());
    ecs::component<Continuous>().with<Body>();
    ecs::component<NativeBody>(ecs::component_hooks<NativeBody>{
        .on_remove = [](ecs_entity_t, NativeBody &native) {
            ecs::resource<RetiredBodies>().ids.push_back(native.body);
        } });
    ecs::observe<ecs::OnSet>().each_for<Position2d>(
        [](ecs::entity body, ecs::observer_event event, const NativeBody &native) {
            const auto &value = *event.trigger_data<Position2d>();
            if (value.x != native.current.x || value.y != native.current.y)
                body.add<PositionChanged>();
        }
    );
    ecs::observe<ecs::OnSet>().each_for<Velocity2d>(
        [](ecs::entity body, ecs::observer_event event, const NativeBody &native) {
            const auto &value = *event.trigger_data<Velocity2d>();
            if (value.x != native.velocity.x || value.y != native.velocity.y)
                body.add<VelocityChanged>();
        }
    );
    ecs::observe<ecs::OnSet>().each_for<Box>([](ecs::entity body, const NativeBody &) {
        body.add<ColliderChanged>();
    });
    ecs::observe<ecs::OnSet>().each_for<Filter>([](ecs::entity body, const NativeBody &) {
        body.add<ColliderChanged>();
    });
    const auto phase = ecs::resource<Phases>().simulate;
    auto release =
        ecs::system("physics.ReleaseBodies")
            .phase(phase)
            .manual()
            .immediate()
            .exclude<Body>()
            .each([](ecs::entity body, const NativeBody &, ecs::target<InWorld>) {
                kill_contacts(body);
                body.remove<NativeBody, ExternalMotion2d>();
            });
    auto create = ecs::system("physics.CreateBodies")
                      .phase(phase)
                      .manual()
                      .immediate()
                      .after(release)
                      .require<Body>()
                      .exclude<NativeBody>()
                      .each([](ecs::entity body,
                               const Position2d &position,
                               const Velocity2d &velocity,
                               ecs::target<InWorld>,
                               ecs::res<const NativeWorld> world) {
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
    auto collider =
        ecs::system("physics.UpdateColliders")
            .phase(phase)
            .manual()
            .immediate()
            .after(create)
            .require<ColliderChanged>()
            .each([](ecs::entity body,
                     NativeBody &native,
                     const Box &box,
                     const Filter &filter,
                     ecs::target<InWorld>,
                     ecs::res<const NativeWorld> world) {
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
    auto teleport = ecs::system("physics.Teleport")
                        .phase(phase)
                        .manual()
                        .immediate()
                        .after(collider)
                        .require<PositionChanged>()
                        .each([](ecs::entity body,
                                 NativeBody &native,
                                 const Position2d &position,
                                 ecs::target<InWorld>,
                                 ecs::res<const NativeWorld> world) {
                            b2Body_SetTransform(
                                native.body,
                                world->meters(position.x, position.y),
                                b2Rot_identity
                            );
                            native.current = position;
                            body.remove<PositionChanged>();
                        });
    auto velocity =
        ecs::system("physics.ChangeVelocity")
            .phase(phase)
            .manual()
            .immediate()
            .after(teleport)
            .require<VelocityChanged>()
            .each([](ecs::entity body,
                     NativeBody &native,
                     const Velocity2d &velocity,
                     ecs::target<InWorld>,
                     ecs::res<const NativeWorld> world) {
                b2Body_SetLinearVelocity(native.body, world->meters(velocity.x, velocity.y));
                native.velocity = velocity;
                body.remove<VelocityChanged>();
            });
    ecs::resource<Pipeline>().synchronized =
        ecs::system("physics.DestroyRetiredBodies")
            .phase(phase)
            .manual()
            .immediate()
            .after(velocity)
            .require<Root>()
            .each([](ecs::entity, ecs::target<InWorld>, ecs::res<RetiredBodies> retired) {
                for (auto id : retired->ids)
                    b2DestroyBody(id);
                retired->ids.clear();
            });
}
} // namespace physics::detail
