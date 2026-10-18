#include "players.h"
#include <bit>

namespace game {

void ServerPlayers::import() {
    ecs::import<ServerMovement>();
    ecs::set_resource(Arena{ ecs::entity::create().set(Match{}).add<net::Replicated>() });
    ecs::component<ServerPlayer>().with<Weapon, Invulnerable>();
    // Players join in a deferred batch (see OnConnect), so these mutations follow the add.
    ecs::observe<ecs::OnAdd>().each_for<ServerPlayer>([](ecs::entity player,
                                                         ecs::res<const Arena> arena) {
        unsigned taken = 0;
        ecs::query().each([&](const PlayerSlot &slot) { taken |= 1u << slot.index; });
        const auto slot = std::uint8_t(std::countr_one(taken));
        if (slot >= max_players) {
            player.kill();
            return;
        }
        const bool over = arena->match.get<Match>().phase == std::uint8_t(Phase::over);
        player.set(
            PlayerSlot{ slot },
            spawn_point(slot),
            physics::Box{ player_half_size },
            player_filter,
            Health{ std::uint8_t(over ? 0 : max_health), max_health }
        );
    });
}
} // namespace game
