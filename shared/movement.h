#pragma once

#include "game.h"
#include <algorithm>
#include <cmath>
#include <sibox2d/physics.h>

namespace game {

struct MoveCommand {
    float x = 0, y = 0;
};
struct Driven {};
/** Added once Health reaches zero; eliminated bodies stop steering. */
struct Eliminated {};

// Box2D filtering is symmetric: a pair collides only if each mask accepts the other.
inline constexpr physics::Filter player_filter{ layer::player, layer::enemy | layer::enemy_shot };
inline constexpr physics::Filter player_shot_filter{ layer::player_shot, layer::enemy };
inline constexpr physics::Filter enemy_filter{ layer::enemy, layer::player | layer::player_shot };
inline constexpr physics::Filter enemy_shot_filter{ layer::enemy_shot, layer::player };

/** Shared movement rule: every PlayerInput becomes a validated held command. Systems
 * that rewrite MoveCommand must be registered before this import to run first. */
struct Kinematics {
    static void import() {
        ecs::import<physics::World>();
        ecs::component<Driven>().with<MoveCommand, Velocity2d>();
        ecs::observe<ecs::OnSet>().each_for<PlayerInput>([](ecs::entity player,
                                                            ecs::observer_event event) {
            const auto &input = *event.trigger_data<PlayerInput>();
            const bool valid = std::isfinite(input.move_x) && std::isfinite(input.move_y) &&
                               std::abs(input.move_x) <= 1 && std::abs(input.move_y) <= 1;
            player.set(valid ? MoveCommand{ input.move_x, input.move_y } : MoveCommand{});
        });
        ecs::observe<ecs::OnSet>().each_for<Health>([](ecs::entity body,
                                                       ecs::observer_event event) {
            if (event.trigger_data<Health>()->current == 0)
                body.add<Eliminated>().set(Velocity2d{});
        });
        ecs::system("game.ApplyMovement")
            .phase(ecs::resource<physics::Phases>().input)
            .manual()
            .require<Driven>()
            .exclude<Eliminated>()
            .each([](const MoveCommand &command, const Position2d &position, Velocity2d &velocity) {
                const float magnitude = std::max(1.0f, std::hypot(command.x, command.y));
                const float step = physics::fixed_step;
                const float reach_x = play_half_width - player_half_size.x;
                const float reach_y = play_half_height - player_half_size.y;
                // Stop at the playfield edge within this step, so velocity matches the motion.
                const float x = std::clamp(
                    position.x + command.x * player_speed / magnitude * step,
                    -reach_x,
                    reach_x
                );
                const float y = std::clamp(
                    position.y + command.y * player_speed / magnitude * step,
                    -reach_y,
                    reach_y
                );
                velocity = { (x - position.x) / step, (y - position.y) / step };
            });
    }
};
} // namespace game
