#pragma once

#include "players.h"
#include <utility>

namespace game {
/** Server-only: shots and enemies die on their first hit; spent ones ignore later contacts. */
struct Expendable {
    bool spent = false;
};
/** Server event, triggered on both endpoints of a contact that counts. */
struct Hit {
    ecs::entity other;
};
/** Firing, despawning and hit resolution. */
struct ServerCombat {
    static void import();
};

/** Spawn a replicated straight-line body of `kind` from its state at completed `tick`. */
template <typename... Extra>
ecs::entity spawn_expendable(
    Kind kind,
    Position2d position,
    Velocity2d velocity,
    std::uint64_t tick,
    Extra &&...extra
) {
    const bool enemy = kind == Kind::enemy;
    const bool player_shot = kind == Kind::player_shot;
    ecs::entity body = ecs::entity::create().set(
        position,
        velocity,
        physics::Box{ enemy         ? Position2d{ 10, 10 }
                      : player_shot ? Position2d{ 8, 3 }
                                    : Position2d{ 3, 3 } },
        enemy         ? enemy_filter
        : player_shot ? player_shot_filter
                      : enemy_shot_filter,
        EntityKind{ std::uint8_t(kind) },
        Trajectory{ tick, position.x, position.y, velocity.x, velocity.y },
        Expendable{},
        std::forward<Extra>(extra)...
    );
    body.add<physics::Body>();
    // Shots are small and fast, so they are swept instead of sampled at step ends.
    if (!enemy)
        body.add<physics::Continuous>();
    return body.add<net::Replicated>();
}
} // namespace game
