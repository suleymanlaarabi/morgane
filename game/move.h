#pragma once

#include "detect.h"
#include <shared/game.h>

#include <algorithm>
#include <cmath>

// Server-side movement. Players take their speed from PlayerInput. Everything
// else, missiles included, already carries a Velocity. The step stores the
// position from the start of the tick in PreviousPosition, then advances.
// Detection runs afterwards and sweeps missiles across that segment.

namespace game {

inline constexpr float player_speed = 300.f;

inline void steer(ecs::entity player) {
    float x = 0.f;
    float y = 0.f;
    if (auto *input = player.try_get<PlayerInput>()) {
        x = input->move_x;
        y = input->move_y;
    }
    float length = std::sqrt(x * x + y * y);
    if (length > 1.f) {
        x /= length;
        y /= length;
    }
    player.set(Velocity{ x * player_speed, y * player_speed });
}

inline void move_body(ecs::entity entity, float dt, const Playfield &field) {
    if (!entity.is_alive() || entity.has<PendingDestroy>())
        return;
    auto *position = entity.try_get<Position>();
    auto *velocity = entity.try_get<Velocity>();
    if (!position || !velocity)
        return;

    Position next{ position->x + velocity->x * dt, position->y + velocity->y * dt };
    if (entity.has<Player>()) {
        float half_width = 0.f;
        float half_height = 0.f;
        if (auto *box = entity.try_get<Collider>()) {
            half_width = box->half_width;
            half_height = box->half_height;
        }
        float min_x = field.min_x + half_width;
        float max_x = field.max_x - half_width;
        float min_y = field.min_y + half_height;
        float max_y = field.max_y - half_height;
        if (min_x <= max_x)
            next.x = std::clamp(next.x, min_x, max_x);
        if (min_y <= max_y)
            next.y = std::clamp(next.y, min_y, max_y);
    }
    entity.set(PreviousPosition{ position->x, position->y }, next);
}

// `after_spawn` is the missile spawn system. Movement runs after it, still
// before detection, so a shot fired this tick travels on the same tick.
inline ecs_system_id_t install_movement(ecs_system_id_t after_spawn = 0) {
    if (!ecs::has_resource<Playfield>())
        ecs::set_resource(Playfield{});

    auto steered = ecs::system("game.Steer")
                       .phase(EcsOnUpdate)
                       .immediate()
                       .require<Player>()
                       .exclude<PendingDestroy>()
                       .each([](ecs::entity player) { steer(player); });

    auto moving = ecs::system("game.Move")
                      .phase(EcsOnUpdate)
                      .immediate()
                      .exclude<PendingDestroy>()
                      .after(steered);
    if (after_spawn != 0)
        moving.after(after_spawn);
    return moving.each([](ecs::entity entity, const Position &, const Velocity &,
                          ecs::res<const DeltaTime> time, ecs::res<const Playfield> field) {
        move_body(entity, time->value, *field);
    });
}

} // namespace game
