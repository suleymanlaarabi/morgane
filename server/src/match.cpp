#include "match.h"
#include "gameplay.h"
#include <vector>

namespace game {
namespace {
void clear_arena() {
    ecs::query().each([](ecs::entity body, const Expendable &) { body.kill(); });
}

void restart_wave(WaveDirector &director, std::uint64_t tick, const GameplayConfig &config) {
    director = { .next_spawn = tick + 1 + config.wave_delay };
}
} // namespace

void ServerMatch::import() {
    ecs::import<ServerWaves>();
    ecs::set_resource(Roster{});
    const auto &phases = ecs::resource<physics::Phases>();

    // Votes arrive on the voter's connection, which owns its player.
    ecs::observe<RestartVote>().each(
        [](ecs::entity connection, ecs::observer_event event, ecs::res<const Arena> arena) {
            if (arena->match.get<Match>().phase != std::uint8_t(Phase::over))
                return;
            const bool ready = event.trigger_data<RestartVote>()->ready;
            // Copy first: tagging moves the players, which may invalidate the borrowed view.
            const auto view = connection.sources<net::OwnedBy>();
            for (auto player : std::vector<ecs::entity>(view.begin(), view.end())) {
                if (ready)
                    player.add<WantsRestart>();
                else
                    player.remove<WantsRestart>();
            }
        }
    );
    // Runs before the player entity dies with its connection, so its slot is still readable.
    ecs::observe<net::OnDisconnect>().each(
        [](ecs::entity connection, ecs::observer_event event, ecs::res<const Arena> arena) {
            const auto reason = std::uint8_t(*event.trigger_data<net::DisconnectReason>());
            for (auto player : connection.sources<net::OwnedBy>())
                if (const auto *slot = player.try_get<PlayerSlot>()) {
                    const PlayerLeft left{ slot->index, reason };
                    ecs::trigger<PlayerLeft>(arena->match, &left);
                }
        }
    );

    ecs::system("game.ForgetRoster").phase(phases.input).manual().each([](ecs::res<Roster> roster) {
        *roster = {};
    });
    ecs::system("game.CountPlayers")
        .phase(phases.input)
        .manual()
        .require<ServerPlayer, PlayerSlot>()
        .each([](ecs::entity, ecs::res<Roster> roster) { ++roster->players; });
    ecs::system("game.CountVotes")
        .phase(phases.input)
        .manual()
        .require<ServerPlayer, WantsRestart>()
        .each([](ecs::entity, ecs::res<Roster> roster) { ++roster->ready; });
    // set(), not an in-place write: Match is replicated and must notify the network.
    ecs::system("game.UpdateMatch")
        .phase(phases.input)
        .manual()
        .each([](ecs::entity match_entity,
                 const Match &match,
                 ecs::res<const Roster> roster,
                 ecs::res<const AlivePlayers> alive,
                 ecs::res<WaveDirector> director,
                 ecs::res<const physics::Clock> clock,
                 ecs::res<const GameplayConfig> config) {
            Match next = match;
            const auto phase = Phase(match.phase);
            if (!roster->players) {
                if (phase != Phase::waiting) {
                    clear_arena();
                    restart_wave(*director, clock->tick, *config);
                }
                next.phase = std::uint8_t(Phase::waiting);
            } else if (phase == Phase::waiting) {
                next.phase = std::uint8_t(Phase::playing);
            } else if (phase == Phase::playing && !alive->count) {
                next.phase = std::uint8_t(Phase::over);
            } else if (phase == Phase::over && roster->ready == roster->players) {
                clear_arena();
                restart_wave(*director, clock->tick, *config);
                ecs::query().each(
                    [&](ecs::entity player, const ServerPlayer &, const PlayerSlot &slot) {
                        player.remove<WantsRestart>().set(
                            spawn_point(slot.index),
                            Velocity2d{},
                            player_filter,
                            Weapon{},
                            Invulnerable{ clock->tick + config->invulnerability },
                            Health{ max_health, max_health }
                        );
                    }
                );
                next.phase = std::uint8_t(Phase::playing);
            }
            next.players = std::uint8_t(roster->players);
            next.ready = next.phase == std::uint8_t(Phase::over) ? std::uint8_t(roster->ready) : 0;
            next.wave = director->wave;
            if (next != match)
                match_entity.set(next);
        });
}
} // namespace game
