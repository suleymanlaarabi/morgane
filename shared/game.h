#pragma once

#include <cstdint>
#include <sinet/net.h>

namespace game {

enum class Action : std::uint8_t { up, down, left, right, fire, count };

/** Client -> server, republished every frame on the owned player. */
struct PlayerInput {
    reflected(float move_x; float move_y; bool fire;)
};

inline constexpr float player_speed = 180;
inline constexpr float input_timeout = 0.25f;
enum Collision : std::uint64_t { Player = 1, Enemy = 2, PlayerShot = 4, EnemyShot = 8 };

/** Server -> clients. A position and velocity from one completed 60 Hz tick. */
struct AuthoritativeMotion {
    reflected(uint64_t tick; float x; float y; float vx; float vy; uint32_t epoch;)
};

/** Network schema; must be called identically on client and server. */
inline void register_network() {
    net::input<PlayerInput>().owner_only();
    net::replicate<AuthoritativeMotion>();
}

} // namespace game
