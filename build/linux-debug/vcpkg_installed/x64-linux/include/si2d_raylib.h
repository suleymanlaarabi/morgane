#ifndef SI2D_RAYLIB_H
#define SI2D_RAYLIB_H

/* This generated file contains includes for project dependencies. */
#include "si2d_raylib/bake_config.h"
#include <raylib.h>
#include <si2d.h>
#include <siecs_scene.h>

/*
 * raylib renderer for si2d. The host owns the window and BeginDrawing /
 * EndDrawing; this module draws the world inside them.
 *
 * Textures load on first use from the asset path (ecs_asset_path) resolved
 * against the scene root. Missing textures draw as a magenta placeholder.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* auto_draw adds an EcsOnRender system drawing with the main camera, for
 * games. Tools that render into their own viewport leave it off and call
 * si2d_raylib_draw. */
ECS_MODULE_DECLARE(si2d_raylib, { bool auto_draw; });

/* Texture of an asset, loaded once. Returns a texture with id 0 when the
 * asset is unknown or cannot be loaded. Requires a window. */
SIECS_API Texture2D si2d_raylib_texture(ecs_asset_t asset);

/* Release every cached texture. Call before CloseWindow when the window
 * closes before the world is finalized. */
SIECS_API void si2d_raylib_unload_textures(void);

/* Camera of the main Camera2d entity for a screen size; false when none. */
SIECS_API bool si2d_raylib_camera(int screen_width, int screen_height, Camera2D *out);

/* Draw every sprite and shape, lowest layer first, culled against the area
 * visible through camera on a screen of the given size. */
SIECS_API void si2d_raylib_draw(Camera2D camera, int screen_width, int screen_height);

/* World-space axis-aligned bounds of a sprite or shape entity. */
SIECS_API bool si2d_raylib_bounds(ecs_entity_t entity, Rectangle *out);

/* Whether a world point lies on a sprite or shape entity, rotation included. */
SIECS_API bool si2d_raylib_hit(ecs_entity_t entity, Vector2 point);

static inline Color si2d_raylib_color(si2d_color_t color) { return (Color){ color.r, color.g, color.b, color.a }; }

#ifdef __cplusplus
}
#endif

#endif
