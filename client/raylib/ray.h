#pragma once

#include <cstdint>
#include <raylib.h>
#include <siecs.h>
#include <siecs_spatial.h>
#include <string>
#include <string_view>

namespace ray {

struct WindowConfig {
    int width = 1280;
    int height = 720;
    std::string title = "Morgane";
    int target_fps = 60;
    Color clear = BLACK;
    bool resizable = false;
};

/** Module: imports sispatial, opens the window and drives BeginDrawing/EndDrawing. */
struct Window {
    static void import(const WindowConfig &config);
};

/** Resource: current framebuffer size, refreshed every frame. */
struct Screen {
    int width;
    int height;
};

/** Handle to a GPU texture owned by the window module. */
struct Texture {
    std::uint32_t id;
};

/** Load (or reuse) a texture. Call after importing ray::Window. */
Texture load_texture(std::string_view path);

/** Draws the texture at the entity global transform (sispatial, added automatically). */
struct Sprite {
    Texture texture;
    Rectangle source{}; // empty = whole texture
    Vector2 pivot{ 0.5f, 0.5f };
    Color tint = WHITE;
    bool flip_x = false;
    bool flip_y = false;
};

/** Draw sprites and cameras here instead of at the global transform (e.g. smoothed motion). */
struct PresentationPosition2d {
    float x = 0, y = 0;
};

/** 2D camera looking at the entity global transform (sispatial, added automatically). */
struct Camera {
    Vector2 offset{};
    float zoom = 1;
    bool centered = true; // offset is relative to the screen center
};

/** Resource: camera used to render sprites this frame. */
struct View {
    Camera2D camera;
    bool active;
};

Vector2 screen_to_world(Vector2 point);
Vector2 world_to_screen(Vector2 point);

} // namespace ray
