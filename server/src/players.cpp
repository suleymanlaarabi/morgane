#include "players.h"
#include <bit>

namespace game {

void ServerPlayers::import() {
    ecs::import<ServerMovement>();
    ecs::component<ServerPlayer>().with<Weapon, Invulnerable>();
    // Players join in a deferred batch (see OnConnect), so these mutations follow the add.
    ecs::observe<ecs::OnAdd>().each_for<ServerPlayer>([](ecs::entity player) {
        unsigned taken = 0;
        ecs::query().each([&](const PlayerSlot &slot) { taken |= 1u << slot.index; });
        const auto slot = std::uint8_t(std::countr_one(taken));
        if (slot >= max_players) {
            player.kill();
            return;
        }
        player.set(
            PlayerSlot{ slot },
            Position2d{ -play_half_width + 48, -60 + float(slot) * 40 },
            physics::Box{ player_half_size },
            player_filter,
            Health{ max_health, max_health }
        );
    });
}
} // namespace game
