#pragma once

#include "components.h"
#include <shared/game.h>

#include <algorithm>
#include <cstdint>

// Server-side player shots. The client only publishes PlayerInput.fire.
// The missile is spawned in front of the ship and owned by the same connection,
// so a disconnect removes it with the player.

namespace game {

inline constexpr float fire_interval = 0.25f;
inline constexpr float missile_speed = 400.f;
inline constexpr float missile_half_width = 8.f;
inline constexpr float missile_half_height = 3.f;
inline constexpr int missile_damage = 1;
inline constexpr int player_health = 3;
inline constexpr float player_half_width = 16.f;
inline constexpr float player_half_height = 8.f;

// Time left before this ship can shoot again. Server only.
struct FireCooldown {
    float remaining = 0.f;
};

struct PlayerRoster {
    uint8_t next = 0;
};

// Players that passed the cooldown check this frame. The shot itself is created
// by a following system, because a relation set inside a deferred system loses
// its source index and the missile can no longer be destroyed.
struct PendingShots {
    ecs_entity_t players[max_players]{};
    uint8_t count = 0;
};

inline ecs::entity spawn_player(ecs::entity connection) {
    if (!ecs::has_resource<PlayerRoster>())
        ecs::set_resource(PlayerRoster{});
    auto &roster = ecs::resource<PlayerRoster>();
    uint8_t slot = static_cast<uint8_t>(roster.next % max_players);
    roster.next = static_cast<uint8_t>(roster.next + 1);

    float y = 200.f + static_cast<float>(slot) * 160.f;
    return ecs::entity::create()
        .set(Position{ 80.f, y })
        .set(Collider{ player_half_width, player_half_height })
        .set(player_mask)
        .set(Health{ player_health })
        .set(PlayerSlot{ slot })
        .set(FireCooldown{})
        .add<Player, net::Replicated>()
        .relate<net::OwnedBy>(connection);
}

inline ecs::entity spawn_player_missile(ecs::entity player) {
    auto position = player.get<Position>();
    auto hull = player.get<Collider>();
    float x = position.x + hull.half_width + missile_half_width;
    float y = position.y;
    int damage = missile_damage;
    if (auto *power = player.try_get<Power>())
        damage += power->level;

    auto missile = ecs::entity::create()
                       .set(Position{ x, y })
                       .set(PreviousPosition{ x, y })
                       .set(Velocity{ missile_speed, 0.f })
                       .set(Collider{ missile_half_width, missile_half_height })
                       .set(player_missile_mask)
                       .set(Damage{ damage })
                       .add<PlayerMissile, net::Replicated>();
    auto owner = player.target<net::OwnedBy>();
    if (owner.id() != 0)
        missile.relate<net::OwnedBy>(owner);
    if (player.has_relation<net::InInstance>())
        missile.relate<net::InInstance>(player.target<net::InInstance>());
    return missile;
}

// True when this step should spawn a missile. The cooldown is written either way.
inline bool arm_shot(ecs::entity player, float dt) {
    if (!player.is_alive() || player.has<PendingDestroy>() || !player.has<Player>())
        return false;
    auto *input = player.try_get<PlayerInput>();
    if (!input || !player.try_get<Position>() || !player.try_get<Collider>())
        return false;

    FireCooldown cooldown{};
    if (auto *existing = player.try_get<FireCooldown>())
        cooldown = *existing;
    if (cooldown.remaining > 0.f)
        cooldown.remaining = std::max(0.f, cooldown.remaining - dt);
    if (!input->fire || cooldown.remaining > 0.f) {
        player.set(cooldown);
        return false;
    }

    cooldown.remaining = fire_interval;
    player.set(cooldown);
    return true;
}

// Returns the new missile, or null when the ship cannot shoot this step.
inline ecs::entity try_fire(ecs::entity player, float dt) {
    if (!arm_shot(player, dt))
        return {};
    return spawn_player_missile(player);
}

inline void collect_shot(ecs::entity player, float dt, PendingShots &pending) {
    if (!arm_shot(player, dt) || pending.count >= max_players)
        return;
    pending.players[pending.count++] = player.id();
}

inline void spawn_pending_shots(ecs_iter_t *) {
    if (!ecs::has_resource<PendingShots>())
        return;
    auto &pending = ecs::resource<PendingShots>();
    uint8_t count = pending.count;
    pending.count = 0;
    for (uint8_t i = 0; i < count; ++i) {
        ecs::entity player = ecs::entity::from(pending.players[i]);
        if (player.is_alive() && !player.has<PendingDestroy>())
            spawn_player_missile(player);
    }
}

inline ecs_system_id_t install_weapons() {
    if (!ecs::has_resource<PendingShots>())
        ecs::set_resource(PendingShots{});

    auto queued = ecs::system("game.Fire")
                      .phase(EcsOnUpdate)
                      .immediate()
                      .require<Player>()
                      .each([](ecs::entity player, ecs::res<PendingShots> pending,
                               ecs::res<const DeltaTime> time) {
                          collect_shot(player, time->value, *pending);
                      });

    // No entity query: this is allowed to mutate the world immediately, which
    // keeps the OwnedBy source index of the new missile valid.
    ecs_system_desc_t spawn{
        .name = "game.SpawnMissiles",
        .callback = spawn_pending_shots,
        .phase = EcsOnUpdate,
        .after = { queued },
        .main_thread_only = true,
        .no_defer = true,
    };
    return ecs_system_init(&spawn);
}

} // namespace game
