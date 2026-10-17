#include "combat.h"
#include "gameplay.h"
#include <cmath>

namespace game {

void ServerCombat::import() {
    ecs::import<ServerPlayers>();
    const auto &phases = ecs::resource<physics::Phases>();
    // Input runs before the step's tick increments, so the spawn state belongs to the last
    // completed tick and trajectory_at(trajectory, Clock.tick) holds after every later step.
    // Reads FireCommand after ExpireMovement rewrote it, an ordering the scheduler derives.
    ecs::system("game.FireWeapons")
        .phase(phases.input)
        .manual()
        .require<ServerPlayer>()
        .exclude<Eliminated>()
        .each([](const FireCommand &command,
                 Weapon &weapon,
                 const Position2d &position,
                 ecs::res<const physics::Clock> clock,
                 ecs::res<const GameplayConfig> config) {
            if (!command.held || clock->tick < weapon.ready_tick)
                return;
            weapon.ready_tick = clock->tick + config->fire_interval;
            spawn_expendable(
                Kind::player_shot,
                { position.x + 18, position.y },
                { config->shot_speed, 0 },
                clock->tick
            );
        });
    ecs::system("game.DespawnOutside")
        .phase(phases.output)
        .manual()
        .require<Trajectory>()
        .each([](ecs::entity body,
                 const Position2d &position,
                 ecs::res<const GameplayConfig> config) {
            if (std::abs(position.x) > play_half_width + config->despawn_margin ||
                std::abs(position.y) > play_half_height + config->despawn_margin)
                body.kill();
        });
    // Contacts arrive in one deferred batch, so kills land after it. Spent is written in
    // place and read back immediately, so an expendable body takes at most one hit.
    ecs::observe<physics::OnContact>().each([](ecs::entity, ecs::observer_event event) {
        const auto &contact = *event.trigger_data<physics::Contact>();
        for (auto endpoint : { contact.a, contact.b })
            if (const auto *expendable = endpoint.try_get<Expendable>();
                expendable && expendable->spent)
                return;
        const Hit on_a{ contact.b }, on_b{ contact.a };
        ecs::trigger<Hit>(contact.a, &on_a);
        ecs::trigger<Hit>(contact.b, &on_b);
    });
    ecs::observe<Hit>().each([](ecs::entity body, Expendable &expendable) {
        expendable.spent = true;
        body.kill();
    });
    // Eliminated players lose their collision mask, and later hits in the eliminating batch
    // fall inside the new invulnerability window, so health cannot underflow. (Observer
    // builders cannot take exclude<>: it returns a plain query, whose each() runs at once.)
    ecs::observe<Hit>().each([](ecs::entity player,
                                Invulnerable &invulnerable,
                                const Health &health,
                                ecs::res<const physics::Clock> clock,
                                ecs::res<const GameplayConfig> config) {
        if (clock->tick < invulnerable.until_tick)
            return;
        invulnerable.until_tick = clock->tick + config->invulnerability;
        player.set(Health{ std::uint8_t(health.current - 1), health.max });
    });
    ecs::observe<ecs::OnAdd>().each_for<Eliminated>([](ecs::entity player, const ServerPlayer &) {
        player.set(physics::Filter{ layer::player, 0 });
    });
}
} // namespace game
