#include "movement.h"

namespace game {
namespace {
struct SpawnedFor {};
} // namespace

void ServerMovement::import() {
    ecs::import<physics::World>();
    ecs::relation<SpawnedFor>({ .storage = EcsRelationByTarget,
                                .on_delete_target = EcsDeleteSources });
    ecs::component<ServerPlayer>()
        .with<physics::Body, Driven, ReceivedAt, FireCommand, AuthoritativeMotion>();
    // Relations come first, so a player refused in ServerPlayer's OnAdd dies last.
    ecs::observe<net::OnConnect>().each([](ecs::entity connection) {
        ecs::entity::create()
            .relate<net::OwnedBy>(connection)
            .relate<SpawnedFor>(connection)
            .add<ServerPlayer, net::Replicated>();
    });
    ecs::observe<ecs::OnSet>().each_for<PlayerInput>(
        [](ecs::entity player, ecs::observer_event event, const ServerPlayer &) {
            player.set(
                ReceivedAt{ ecs::resource<physics::Clock>().tick },
                FireCommand{ event.trigger_data<PlayerInput>()->fire }
            );
        }
    );
    // Registered before Kinematics, so ApplyMovement reads the expired command this tick.
    ecs::system("game.ExpireMovement")
        .phase(ecs::resource<physics::Phases>().input)
        .manual()
        .require<ServerPlayer>()
        .each([](const ReceivedAt &received,
                 MoveCommand &move,
                 FireCommand &fire,
                 ecs::res<const physics::Clock> clock) {
            if (clock->tick - received.tick >= input_timeout / physics::fixed_step) {
                move = {};
                fire = {};
            }
        });
    ecs::import<Kinematics>();
    // set(), not an in-place write: AuthoritativeMotion is replicated and must notify the network.
    ecs::system("game.PublishAuthoritativeMotion")
        .phase(ecs::resource<physics::Phases>().output)
        .manual()
        .require<ServerPlayer>()
        .each([](ecs::entity player,
                 const Position2d &position,
                 const Velocity2d &velocity,
                 const AuthoritativeMotion &previous,
                 ecs::res<const physics::Clock> clock) {
            const auto error = std::hypot(
                position.x - previous.x - velocity.x * physics::fixed_step,
                position.y - previous.y - velocity.y * physics::fixed_step
            );
            const auto epoch = previous.epoch + unsigned(previous.tick && error > 0.02f);
            player.set(
                AuthoritativeMotion{ clock->tick,
                                     position.x,
                                     position.y,
                                     velocity.x,
                                     velocity.y,
                                     epoch }
            );
        });
}
} // namespace game
