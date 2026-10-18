#pragma once

#include <sinet/net.h>
#include <cstdint>

namespace game {

enum class Action : std::uint8_t { up, down, left, right, fire, count };

/** Client -> server, republished every frame on the owned player. */
struct PlayerInput {
    reflected(float move_x; float move_y; bool fire;)
};

/** Network schema; must be called identically on client and server. */
inline void register_network() {
    net::input<PlayerInput>().owner_only();
}

} // namespace game
