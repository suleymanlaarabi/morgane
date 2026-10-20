#pragma once

/*
 * Scene contract for morgane. The editor compiles this header as a plugin;
 * the client and the server call morgane_contract() before loading a scene.
 * Types that live in sinet stay behind SIECS_EDITOR_PLUGIN so the editor
 * never depends on the game's network stack: JSON still keys them by the
 * same qualified name.
 */

#include <si2d.h>
#include <siecs_editor/contract.hpp>
#include <cstdint>

#ifdef SIECS_EDITOR_PLUGIN
namespace net {
struct Replicated {};
}
#else
#include <sinet/net.h>
#endif

namespace game {

/** Filled by the scene; the server places a joining player on slot. */
struct SpawnPoint {
    reflected(uint8_t slot;)
};

} // namespace game

SIECS_CONTRACT(morgane) {
    ecs::import<si2d>();
    ecs::component<net::Replicated>();
    siecs::meta<game::SpawnPoint>().category("Gameplay").range("slot", 0, 3);
}
