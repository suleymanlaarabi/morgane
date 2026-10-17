#include "waves.h"
#include "gameplay.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace game {

void ServerWaves::import() {
    ecs::import<ServerCombat>();
    const auto &config = ecs::resource<GameplayConfig>();
    std::seed_seq seed{ std::uint32_t(config.seed), std::uint32_t(config.seed >> 32) };
    ecs::set_resource(Rng{ std::mt19937(seed) });
    ecs::set_resource(WaveDirector{ .next_spawn = config.wave_delay });
    ecs::set_resource(AlivePlayers{});
    const auto &phases = ecs::resource<physics::Phases>();
    ecs::system("game.ForgetAlivePlayers")
        .phase(phases.input)
        .manual()
        .each([](ecs::res<AlivePlayers> alive) { alive->count = 0; });
    ecs::system("game.GatherAlivePlayers")
        .phase(phases.input)
        .manual()
        .require<ServerPlayer>()
        .exclude<Eliminated>()
        .each([](const Position2d &position, ecs::res<AlivePlayers> alive) {
            // A refused join outnumbers the slots until its batch kills it.
            if (alive->count < alive->positions.size())
                alive->positions[alive->count++] = position;
        });
    ecs::system("game.SpawnWaves")
        .phase(phases.input)
        .manual()
        .each([](ecs::res<WaveDirector> director,
                 ecs::res<Rng> rng,
                 ecs::res<const AlivePlayers> alive,
                 ecs::res<const physics::Clock> clock,
                 ecs::res<const GameplayConfig> config) {
            const auto tick = clock->tick;
            if (!alive->count) {
                // A player present from the next tick meets enemies wave_delay later.
                director->next_spawn = tick + 1 + config->wave_delay;
                director->remaining = 0;
                return;
            }
            if (tick < director->next_spawn)
                return;
            if (!director->remaining) {
                director->remaining = config->wave_size;
                ++director->wave;
            }
            const float reach = play_half_height - 24;
            const Position2d position{
                play_half_width + config->spawn_offset,
                std::uniform_real_distribution<float>(-reach, reach)(rng->engine)
            };
            auto warmup = std::uniform_int_distribution<std::uint64_t>(30, 120);
            spawn_expendable(
                Kind::enemy,
                position,
                { -config->enemy_speed, 0 },
                tick,
                EnemyGun{ tick + warmup(rng->engine) }
            );
            // Spacing is a distance along the enemies' path, so it becomes whole ticks.
            const auto spacing = std::uint64_t(
                std::ceil(config->spawn_spacing / (config->enemy_speed * physics::fixed_step))
            );
            director->next_spawn = tick + (--director->remaining ? spacing : config->wave_interval);
        });
    ecs::system("game.EnemyFire")
        .phase(phases.input)
        .manual()
        .each([](EnemyGun &gun,
                 const Position2d &position,
                 ecs::res<const AlivePlayers> alive,
                 ecs::res<const physics::Clock> clock,
                 ecs::res<const GameplayConfig> config) {
            if (position.x > play_half_width || clock->tick < gun.next_tick)
                return;
            float nearest = std::numeric_limits<float>::infinity();
            Position2d target{};
            for (unsigned i = 0; i < alive->count; ++i) {
                const auto &player = alive->positions[i];
                const auto distance = std::hypot(player.x - position.x, player.y - position.y);
                if (distance < nearest) {
                    nearest = distance;
                    target = player;
                }
            }
            if (!std::isfinite(nearest))
                return;
            const auto speed = config->enemy_shot_speed / std::max(nearest, 1e-3f);
            spawn_expendable(
                Kind::enemy_shot,
                position,
                { (target.x - position.x) * speed, (target.y - position.y) * speed },
                clock->tick
            );
            gun.next_tick = clock->tick + config->enemy_fire_interval;
        });
}
} // namespace game
