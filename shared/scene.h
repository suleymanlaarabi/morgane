#pragma once

#include "contract.hpp"
#include <siecs_scene.h>
#include <filesystem>
#include <string>

namespace game {

/** Directory that contains `scenes/` and `assets/`, walking up from cwd. */
inline std::filesystem::path game_root() {
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::current_path(ec);
    for (int i = 0; i < 6 && !ec; i++) {
        if (std::filesystem::exists(dir / "scenes" / "main.scene.json", ec))
            return dir;
        if (!dir.has_parent_path() || dir == dir.parent_path())
            break;
        dir = dir.parent_path();
    }
    return std::filesystem::current_path();
}

/**
 * Load `scenes/main.scene.json` once. The server keeps every entity. The
 * client skips entities tagged net::Replicated before they are created
 * (`skip_with`), so replicated actors never exist locally until the
 * snapshot arrives.
 */
inline bool load_level(bool client) {
    ecs::import<siecs_scene>();
    morgane_contract();
    const std::filesystem::path root = game_root();
    ecs_scene_set_root(root.string().c_str());
    ecs_asset_scan("assets");
    ecs_scene_load_desc_t desc{};
    desc.keep_ids = true;
    if (client)
        desc.skip_with = ecs::component<net::Replicated>().id();
    const auto cooked = root / "scenes" / "main.scene.siecsscn";
    const auto json = root / "scenes" / "main.scene.json";
    std::error_code exists_ec;
    const auto path =
        std::filesystem::exists(cooked, exists_ec) ? cooked : json;
    return ecs_scene_load_file(path.string().c_str(), &desc, nullptr);
}

} // namespace game
