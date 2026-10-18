#pragma once

#include <sinet/net.h>

#include <cstdint>

// Collision data shared by the client and the server.
//
// Replicated components are registered in register_components(). Both programs
// must call it, in this order, before ecs::run(). Everything else stays on the
// server: the client draws the result, it does not decide contacts.

namespace game {

using std::uint8_t;

inline constexpr uint8_t max_players = 4;

// One bit per role. A contact exists when one entity's `hits` overlaps the
// other's `belongs`. Only the seeker lists the target, so a pair is stored once.
namespace layer {

inline constexpr uint8_t player = 1u << 0;
inline constexpr uint8_t monster = 1u << 1;
inline constexpr uint8_t player_missile = 1u << 2;
inline constexpr uint8_t enemy_missile = 1u << 3;
inline constexpr uint8_t obstacle = 1u << 4;
inline constexpr uint8_t destructible = 1u << 5;
inline constexpr uint8_t powerup = 1u << 6;

} // namespace layer

constexpr uint8_t layers(uint8_t a, uint8_t b = 0, uint8_t c = 0, uint8_t d = 0, uint8_t e = 0) {
    return static_cast<uint8_t>(a | b | c | d | e);
}

// Center of the entity. The next update replaces the previous one.
struct Position {
    reflected(float x; float y;)
};

// Axis-aligned box centered on Position. Half extents, so a tile of size 32
// stores 16 and 16. Server only.
struct Collider {
    reflected(float half_width; float half_height;)
};

struct CollisionMask {
    reflected(uint8_t belongs; uint8_t hits;)
};

// Units per second. The simulation moves, then tests. Server only.
struct Velocity {
    reflected(float x; float y;)
};

// Position at the start of the step. A fast missile is tested along the
// segment from here to Position, so it cannot cross a target in one tick.
struct PreviousPosition {
    reflected(float x; float y;)
};

// Remaining life of this entity. Each player has its own value.
struct Health {
    reflected(int value;)
};

// Damage this entity deals on contact. Not the life it has left. Server only.
struct Damage {
    reflected(int value;)
};

// Weapon bonus. A power-up raises it by one. Clients must see the same level.
struct Power {
    reflected(int level;)
};

// Reliable notice that a player reached 0 health. `slot` is the PlayerSlot index.
struct PlayerKilled {
    reflected(uint8_t slot;)
};

// A monster, missile, tile, or other non-player was removed by a hit.
struct Destroyed {
    reflected(uint8_t layer;)
};

// A bonus appeared where a monster died. The power-up entity is spawned as well.
struct BonusSpawned {
    reflected(float x; float y;)
};

// The player took a power-up. `level` is the Power value after the pickup.
struct PickedUp {
    reflected(uint8_t slot; int level;)
};

// The owning client left. Other clients drop that player; the simulation continues.
struct PlayerLeft {
    reflected(uint8_t slot;)
};

// Stable player index, 0 .. max_players-1. Chosen at spawn and never written
// by a hit. The client maps it to a color or a sprite.
struct PlayerSlot {
    reflected(uint8_t index;)
};

// Role tags. DropsPowerup is added beside Monster for the enemy that leaves a bonus.
struct Player {};
struct Monster {};
struct DropsPowerup {};
struct PlayerMissile {};
struct EnemyMissile {};
struct Obstacle {};
struct DestructibleTile {};
struct Powerup {};

// Set when this entity must not be hit again. Removed at the end of the step.
struct PendingDestroy {};

inline constexpr CollisionMask player_mask{
    .belongs = layer::player,
    .hits = layers(layer::monster, layer::enemy_missile, layer::obstacle, layer::destructible,
                   layer::powerup),
};

inline constexpr CollisionMask monster_mask{
    .belongs = layer::monster,
    .hits = 0,
};

inline constexpr CollisionMask player_missile_mask{
    .belongs = layer::player_missile,
    .hits = layers(layer::monster, layer::destructible),
};

inline constexpr CollisionMask enemy_missile_mask{
    .belongs = layer::enemy_missile,
    .hits = layer::player,
};

inline constexpr CollisionMask obstacle_mask{
    .belongs = layer::obstacle,
    .hits = 0,
};

inline constexpr CollisionMask destructible_mask{
    .belongs = layer::destructible,
    .hits = 0,
};

inline constexpr CollisionMask powerup_mask{
    .belongs = layer::powerup,
    .hits = 0,
};

inline void register_components() {
    net::replicate<Position>();
    net::replicate<Health>().reliable();
    net::replicate<Power>().reliable();
    net::replicate<PlayerSlot>().reliable();
    net::event<PlayerKilled>().server_to_clients().reliable();
    net::event<Destroyed>().server_to_clients().reliable();
    net::event<BonusSpawned>().server_to_clients().reliable();
    net::event<PickedUp>().server_to_clients().reliable();
    net::event<PlayerLeft>().server_to_clients().reliable();
    net::replicate<Player>().reliable();
    net::replicate<Monster>().reliable();
    net::replicate<DropsPowerup>().reliable();
    net::replicate<PlayerMissile>().reliable();
    net::replicate<EnemyMissile>().reliable();
    net::replicate<Obstacle>().reliable();
    net::replicate<DestructibleTile>().reliable();
    net::replicate<Powerup>().reliable();
}

} // namespace game
