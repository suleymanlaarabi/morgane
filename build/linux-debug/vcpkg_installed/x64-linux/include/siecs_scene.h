#ifndef SIECS_SCENE_H
#define SIECS_SCENE_H

/* This generated file contains includes for project dependencies. */
#include "siecs_scene/bake_config.h"
#include <siecs.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Name-based JSON scenes.
 *
 * A scene file lists entities with their components and relations, all keyed
 * by registered names, so a scene survives registration order changes, new
 * fields and renamed types (see ecs_component_alias). Entities reference each
 * other through scene-local string ids.
 *
 *   {
 *     "format": "siecs.scene", "version": 1,
 *     "entities": [
 *       { "id": "ship", "name": "Ship", "prefab": "prefabs/ship.prefab.json",
 *         "components": { "Position2d": { "x": 80, "y": 200 }, "Player": {} },
 *         "relations": { "ChildOf": "root" } }
 *     ]
 *   }
 *
 * Field encoding: numbers, booleans, enums as enumerator names, char arrays and
 * char * as strings, ecs_entity_t as a scene id string (or null), ecs_asset_t as
 * an asset path string (or null), nested structs as objects and other arrays as
 * arrays. Missing fields keep the component default, unknown fields are ignored
 * with a warning.
 *
 * Every component, relation and prefab used by a scene must be registered or
 * loadable before loading it.
 *
 * Components whose reflected type carries the boolean metadata
 * "siecs.transient" = true (derived state such as GlobalPosition2d) are not
 * saved.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Intern path and return its asset id (see ecs_asset_t). Interning is process-wide and the
 * returned id does not depend on the run, the machine or the load order. */
SIECS_API ecs_asset_t ecs_asset(const char *path);

/* Return the path of an asset id interned in this process, or NULL. */
SIECS_API const char *ecs_asset_path(ecs_asset_t asset);

/* Intern every file below directory (relative to the scene root), so ids
 * received from elsewhere, e.g. a server, resolve to their path. Hidden
 * entries are skipped. Returns the number of files interned. */
SIECS_API uint32_t ecs_asset_scan(const char *directory);

/* Scene-local identity, attached by loads with keep_ids and used by saves.
 * Editors keep it so ids and references stay stable across save cycles. */
ECS_COMPONENT_DECLARE(SceneId, { uint64_t value; });

/* Prefab file path, attached to the root of a loaded prefab definition. */
ECS_COMPONENT_DECLARE(ScenePrefab, { char *path; });

/* Raw JSON objects of components and relations unknown when the entity was
 * loaded with keep_unknown. Saves write them back unchanged. */
ECS_COMPONENT_DECLARE(SceneUnknown, {
    char *components;
    char *relations;
});

/* Format a scene id the way scene files spell it. Returns the length. */
SIECS_API uint32_t ecs_scene_id_format(uint64_t id, char out[16]);

/* A new random, non-zero scene id. */
SIECS_API uint64_t ecs_scene_id_new(void);

/* Entity view passed to load filters before anything is created. */
typedef struct {
    /* Scene id as written in the file, not NUL-terminated. */
    const char *id;
    uint32_t id_length;
    /* Entity name or NULL; NUL-terminated and borrowed for the call. */
    const char *name;
    /* Prefab path or NULL; NUL-terminated and borrowed for the call. */
    const char *prefab;
    /* Known components listed for the entity in the file. */
    const ecs_component_t *components;
    uint32_t component_count;
} ecs_scene_entity_t;

typedef bool (*ecs_scene_filter_t)(const ecs_scene_entity_t *entity, void *ctx);
typedef void (*ecs_scene_warn_t)(const char *message, void *ctx);
typedef void (*ecs_scene_entity_callback_t)(const ecs_scene_entity_t *entity, ecs_entity_t created, void *ctx);

typedef struct {
    /* Keep only entities for which filter returns true. Optional. */
    ecs_scene_filter_t filter;
    void *filter_ctx;
    /* Skip entities listing this component, e.g. a network replication tag on
     * clients that receive those entities from a server. 0 disables. */
    ecs_component_t skip_with;
    /* Keep only entities listing this component. 0 disables. */
    ecs_component_t only_with;
    /* ChildOf target given to loaded entities without a loaded parent. */
    ecs_entity_t parent;
    /* Attach SceneId to loaded entities. */
    bool keep_ids;
    /* Attach SceneUnknown for unknown components and relations. */
    bool keep_unknown;
    /* Load as a prefab definition: abstract entities, root tagged ScenePrefab. */
    bool as_prefab;
    /* Prefab definition path stored in ScenePrefab when as_prefab is set. */
    const char *prefab_path;
    /* Called for every created entity. Optional. */
    ecs_scene_entity_callback_t on_entity;
    void *on_entity_ctx;
    /* Receives non fatal diagnostics. Defaults to stderr. */
    ecs_scene_warn_t warn;
    void *warn_ctx;
} ecs_scene_load_desc_t;

typedef struct {
    uint32_t loaded;
    uint32_t skipped;
    uint32_t warnings;
    /* First created entity without a loaded parent, or 0. */
    ecs_entity_t root;
} ecs_scene_load_result_t;

/* Instantiate a JSON scene into the active world. The document structure is
 * validated before any entity is created, so a failed load leaves the world
 * untouched; problems inside component values (type mismatches, malformed
 * numbers) become warnings and keep the field default. Loads append: existing
 * entities are kept. desc and result may be NULL. */
SIECS_API bool ecs_scene_load_json(
    const char *json,
    size_t length,
    const ecs_scene_load_desc_t *desc,
    ecs_scene_load_result_t *result
);

/* Read and load a scene file. Relative paths resolve against the scene root.
 * Files starting with the SIECSSCN cooked header are unwrapped in place: one
 * read, no extra copy of the JSON payload. */
SIECS_API bool ecs_scene_load_file(
    const char *path,
    const ecs_scene_load_desc_t *desc,
    ecs_scene_load_result_t *result
);

/* Wrap a JSON scene in a SIECSSCN v1 header (magic + version + payload size).
 * The payload stays name-indexed JSON so a cooked file is ABI-portable; the
 * header lets the loader skip format sniffing. Relative paths resolve against
 * the scene root. */
SIECS_API bool ecs_scene_cook_file(const char *json_path, const char *cooked_path);

typedef struct {
    /* Entities to save; NULL saves every entity matched by an empty query
     * (abstract and disabled entities excluded). */
    const ecs_entity_t *entities;
    uint32_t count;
    /* Save the listed entities together with their ChildOf descendants. */
    bool recursive;
    /* Write `"format": "siecs.prefab"` instead of a scene header. */
    bool as_prefab;
} ecs_scene_save_desc_t;

/* Serialize entities to a NUL-terminated JSON document owned by the caller
 * (release with ecs_scene_json_free). length may be NULL. Returns NULL on
 * failure. Unserializable data (raw pointers, unreflected components) is
 * skipped. */
SIECS_API char *ecs_scene_save_json(const ecs_scene_save_desc_t *desc, size_t *length);

SIECS_API bool ecs_scene_save_file(const char *path, const ecs_scene_save_desc_t *desc);

/* Serialize the live world to a cooked SIECSSCN file (header + JSON payload). */
SIECS_API bool ecs_scene_save_cooked_file(const char *path, const ecs_scene_save_desc_t *desc);

SIECS_API void ecs_scene_json_free(char *json);

/* Return the last fatal error message, or NULL. */
SIECS_API const char *ecs_scene_error(void);

/* Directory against which relative scene, prefab and asset paths resolve.
 * Defaults to the working directory. The string is copied. */
SIECS_API void ecs_scene_set_root(const char *directory);
SIECS_API const char *ecs_scene_root(void);

/* Load a prefab definition once per world and return its abstract root. */
SIECS_API ecs_entity_t ecs_prefab_load(const char *path);

/* Create an entity that inherits from the prefab at path, with a copy of the
 * prefab's ChildOf subtree. Returns 0 when the prefab cannot be loaded. */
SIECS_API ecs_entity_t ecs_prefab_spawn(const char *path);

/* Return the prefab path an entity was instantiated from, or NULL. */
SIECS_API const char *ecs_prefab_of(ecs_entity_t entity);

ECS_MODULE_DECLARE(siecs_scene, { uint8_t _unused; });

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string>
#include <string_view>

namespace ecs::scene {

using load_desc = ecs_scene_load_desc_t;
using load_result = ecs_scene_load_result_t;

/** Load a JSON or cooked scene file into the active world. */
inline bool load(const std::string &path, const load_desc &desc = {}, load_result *result = nullptr) {
    return ecs_scene_load_file(path.c_str(), &desc, result);
}

/** Wrap a JSON scene file as SIECSSCN for the runtime. */
inline bool cook(const std::string &json_path, const std::string &cooked_path) {
    return ecs_scene_cook_file(json_path.c_str(), cooked_path.c_str());
}

/** Load a JSON scene from memory into the active world. */
inline bool load_json(std::string_view json, const load_desc &desc = {}, load_result *result = nullptr) {
    return ecs_scene_load_json(json.data(), json.size(), &desc, result);
}

/** Serialize the whole active world, or the listed entities. */
inline std::string save_json(const ecs_scene_save_desc_t &desc = {}) {
    size_t length = 0;
    char *json = ecs_scene_save_json(&desc, &length);
    std::string out = json ? std::string(json, length) : std::string();
    ecs_scene_json_free(json);
    return out;
}

inline bool save(const std::string &path, const ecs_scene_save_desc_t &desc = {}) {
    return ecs_scene_save_file(path.c_str(), &desc);
}

/** Spawn an instance of a prefab definition file. */
inline ecs_entity_t spawn(const std::string &prefab_path) {
    return ecs_prefab_spawn(prefab_path.c_str());
}

/** Intern an asset path. */
inline ecs_asset_t asset(const char *path) { return ecs_asset(path); }

} // namespace ecs::scene
#endif

#endif
