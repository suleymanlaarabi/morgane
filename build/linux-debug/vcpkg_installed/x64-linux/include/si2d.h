#ifndef SI2D_H
#define SI2D_H

/* This generated file contains includes for project dependencies. */
#include "si2d/bake_config.h"
#include <siecs.h>
#include <siecs_spatial.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * 2D presentation data, independent of any renderer.
 *
 * Servers import it to load and replicate scenes; renderers (si2d_raylib) and
 * the scene editor draw it. Every drawable gets the sispatial 2D transform:
 * it is drawn at GlobalPosition2d, rotated by GlobalRotation2d (radians) and
 * scaled by GlobalScale2d.
 */

#ifdef __cplusplus
#define SI2D_DEFAULT(...) = __VA_ARGS__
#else
#define SI2D_DEFAULT(...)
#endif

/* Value types. Tools edit si2d_vec2_t on one line, si2d_rect_t as a
 * rectangle and si2d_color_t ("siecs.role" = "color") with a color picker. */
typedef struct si2d_vec2_t {
    float x;
    float y;
} si2d_vec2_t;

typedef struct si2d_rect_t {
    float x;
    float y;
    float width;
    float height;
} si2d_rect_t;

typedef struct si2d_color_t {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} si2d_color_t;

#define SI2D_WHITE ((si2d_color_t){ 255, 255, 255, 255 })

typedef enum si2d_shape_t {
    SI2D_SHAPE_RECT,
    SI2D_SHAPE_CIRCLE,
} si2d_shape_t;

/* Textured quad. source is a texel rectangle; a zero size uses the whole
 * texture. size overrides the drawn size in world units; zero uses the source
 * size. pivot is the normalized point placed at the position. Higher layers
 * draw on top. */
ECS_COMPONENT_DECLARE_CPP(
    Sprite2d,
    ECS_CPP_FIELDS(
        ecs_asset_t texture;
        si2d_rect_t source;
        si2d_vec2_t size;
        si2d_vec2_t pivot SI2D_DEFAULT({ 0.5f, 0.5f });
        si2d_color_t tint SI2D_DEFAULT({ 255, 255, 255, 255 });
        int32_t layer;
        bool flip_x;
        bool flip_y;
    ),
    ECS_CPP_METHODS()
);

/* Untextured rectangle or circle (size.x is the diameter), centred on the
 * position. Useful for blockouts, triggers and backgrounds. */
ECS_COMPONENT_DECLARE_CPP(
    Shape2d,
    ECS_CPP_FIELDS(
        si2d_shape_t shape;
        si2d_vec2_t size SI2D_DEFAULT({ 32, 32 });
        si2d_color_t color SI2D_DEFAULT({ 255, 255, 255, 255 });
        int32_t layer;
        bool outline;
    ),
    ECS_CPP_METHODS()
);

/* Camera looking at its entity's global transform. offset is in screen
 * pixels, relative to the screen center when centered. The renderer uses the
 * first camera tagged MainCamera2d, else the first camera found. */
ECS_COMPONENT_DECLARE_CPP(
    Camera2d,
    ECS_CPP_FIELDS(
        si2d_vec2_t offset;
        float zoom SI2D_DEFAULT(1.0f);
        bool centered SI2D_DEFAULT(true);
    ),
    ECS_CPP_METHODS()
);

ECS_TAG_DECLARE(MainCamera2d);

ECS_MODULE_DECLARE(si2d, { uint8_t _unused; });

#endif
