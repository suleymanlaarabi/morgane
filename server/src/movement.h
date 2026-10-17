#pragma once

#include <shared/movement.h>

namespace game {
struct ServerPlayer {};
/** Tick of the last PlayerInput; held commands expire after input_timeout. */
struct ReceivedAt {
    std::uint64_t tick = 0;
};
/** Server-only held fire button, validated alongside MoveCommand. */
struct FireCommand {
    bool held = false;
};
/** Systems that consume held commands run after `expired`. */
struct InputPipeline {
    ecs_system_id_t expired = 0;
};
struct ServerMovement {
    static void import();
};
} // namespace game
