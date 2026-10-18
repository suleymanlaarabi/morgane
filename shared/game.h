#pragma once

#include <cstdint>
#include <sibox2d/physics.h>
#include <sinet/net.h>

namespace game {

enum class Action : std::uint8_t { up, down, left, right, fire, count };

/** Client -> server, republished every frame on the owned player. */
struct PlayerInput {
    reflected(float move_x; float move_y; bool fire;)
};

inline constexpr float player_speed = 180;
inline constexpr float input_timeout = 0.25f;
inline constexpr unsigned max_players = 4;
/** The playfield is centred on the origin; entities stay inside it. */
inline constexpr float play_half_width = 320;
inline constexpr float play_half_height = 180;
inline const Position2d player_half_size{ 14, 6 };
inline constexpr std::uint8_t max_health = 3;

/** Collision categories, combined into physics::Filter masks. */
namespace layer {
inline constexpr std::uint64_t player = 1, enemy = 2, player_shot = 4, enemy_shot = 8;
} // namespace layer

/** Server -> clients. A position and velocity from one completed 60 Hz tick. */
struct AuthoritativeMotion {
    reflected(uint64_t tick; float x; float y; float vx; float vy; uint32_t epoch;)
};
/** Server -> clients. Stable player number in [0, max_players). */
struct PlayerSlot {
    reflected(uint8_t index;)
};
struct Health {
    reflected(uint8_t current; uint8_t max;)
};
/** Replicated tags are dropped from spawn snapshots, so non-player kinds are a value. */
enum class Kind : std::uint8_t { player_shot, enemy, enemy_shot };
struct EntityKind {
    reflected(uint8_t value;)
};
/** Server -> clients. Straight-line motion from the state at a 60 Hz tick. */
struct Trajectory {
    reflected(uint64_t tick; float x; float y; float vx; float vy;)
};

inline Position2d trajectory_at(const Trajectory &trajectory, double tick) {
    const auto elapsed = float((tick - double(trajectory.tick)) * physics::fixed_step);
    return { trajectory.x + trajectory.vx * elapsed, trajectory.y + trajectory.vy * elapsed };
}

enum class Phase : std::uint8_t { waiting, playing, over };
/** Server -> clients, on the single match entity. `ready` players voted to play again. */
struct Match {
    reflected(uint8_t phase; uint8_t players; uint8_t ready; uint32_t wave;) bool
    operator==(const Match &) const = default;
};
/** Client -> server event on the client's connection (net::LocalPeer::connection): (un)vote to
 * play again once the match is over. */
struct RestartVote {
    reflected(bool ready;)
};
/** Server -> clients event on the match entity. `reason` is a net::DisconnectReason. */
struct PlayerLeft {
    reflected(uint8_t slot; uint8_t reason;)
};
/** Server -> clients event on a body destroyed by a hit at `tick`, just before it despawns. */
struct Destroyed {
    reflected(uint64_t tick;)
};

/** Network schema; must be called identically on client and server. */
inline void register_network() {
    net::input<PlayerInput>().owner_only();
    net::replicate<AuthoritativeMotion>();
    net::replicate<PlayerSlot>().reliable();
    net::replicate<Health>().reliable();
    net::replicate<EntityKind>().reliable();
    net::replicate<Trajectory>().reliable();
    net::replicate<Match>().reliable();
    net::event<RestartVote>().reliable();
    net::event<PlayerLeft>().server_to_clients().reliable();
    net::event<Destroyed>().server_to_clients().reliable();
}

} // namespace game
