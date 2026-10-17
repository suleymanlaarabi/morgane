#include "movement.h"

namespace game {
namespace {
struct ReceivedAt {
    std::uint64_t tick = 0;
};
struct SpawnedFor {};
} // namespace

void ServerMovement::import() {
    ecs::import<physics::World>();
    ecs::relation<SpawnedFor>({ .storage = EcsRelationByTarget,
                                .on_delete_target = EcsDeleteSources });
    ecs::component<ServerPlayer>().with<physics::Body, Driven, ReceivedAt, AuthoritativeMotion>();
    ecs::observe<net::OnConnect>().each([](ecs::entity connection) {
        ecs::entity::create()
            .add<ServerPlayer, net::Replicated>()
            .relate<net::OwnedBy>(connection)
            .relate<SpawnedFor>(connection)
            .set(
                Position2d{ 0, float(connection.get<net::PeerId>().value % 8) * 30 },
                player_filter
            );
    });
    ecs::observe<ecs::OnSet>().each_for<PlayerInput>([](ecs::entity player, const ServerPlayer &) {
        player.set(ReceivedAt{ ecs::resource<physics::Clock>().tick });
    });
    auto expire =
        ecs::system("game.ExpireMovement")
            .phase(ecs::resource<physics::Phases>().input)
            .manual()
            .immediate()
            .require<ServerPlayer>()
            .each([](ecs::entity player,
                     const ReceivedAt &received,
                     ecs::res<const physics::Clock> clock) {
                if (clock->tick - received.tick >= input_timeout / physics::fixed_step)
                    player.set(MoveCommand{});
            });
    ecs::import<Kinematics>(expire);
    ecs::system("game.PublishAuthoritativeMotion")
        .phase(ecs::resource<physics::Phases>().output)
        .manual()
        .immediate()
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
