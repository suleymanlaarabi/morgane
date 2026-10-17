#include "ray.h"
#include <client/scene/scene_manager.h>
#include <cmath>
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace ray {

/** Resource: textures loaded by load_texture, unloaded before CloseWindow. */
struct Textures {
    std::unordered_map<std::string, std::uint32_t> by_path;
    std::vector<Texture2D> loaded;
};

/** Sprites and cameras get a full sispatial transform so their globals are propagated. */
template <typename T> static void require_transform() {
    ecs::component<T>()
        .template with<
            Position2d,
            Rotation2d,
            Scale2d,
            GlobalPosition2d,
            GlobalRotation2d,
            GlobalScale2d>();
}

static Vector2 draw_position(
    const GlobalPosition2d &position,
    ecs::optional<const PresentationPosition2d> presentation
) {
    return presentation ? Vector2{ presentation->x, presentation->y }
                        : Vector2{ position.x, position.y };
}

void Window::import(const WindowConfig &config) {
    ecs::import<sispatial>();
    require_transform<Sprite>();
    require_transform<Camera>();

    SetConfigFlags(config.resizable ? FLAG_WINDOW_RESIZABLE : 0);
    InitWindow(config.width, config.height, config.title.c_str());
    SetTargetFPS(config.target_fps);
    SetExitKey(KEY_NULL);

    Textures textures;
    ecs::set_resource(config);
    ecs::set_resource(textures);
    ecs::set_resource(Screen{ GetScreenWidth(), GetScreenHeight() });
    ecs::set_resource(View{ Camera2D{ .zoom = 1 }, false });

    ecs::system("ray.WindowEvents")
        .phase(EcsOnLoad)
        .immediate()
        .each([](ecs::res<Screen> screen, ecs::res<View> view) {
            if (WindowShouldClose())
                ecs::quit();
            screen->width = GetScreenWidth();
            screen->height = GetScreenHeight();
            view->active = false;
        });

    ecs::system("ray.BeginFrame")
        .phase(EcsPreRender)
        .immediate()
        .each([](ecs::res<const WindowConfig> config) {
            BeginDrawing();
            ClearBackground(config->clear);
        });

    auto camera = ecs::system("ray.BeginCamera")
                      .phase(EcsOnRender)
                      .immediate()
                      .each([](const Camera &camera,
                               const GlobalPosition2d &position,
                               const GlobalRotation2d &rotation,
                               ecs::optional<const PresentationPosition2d> presentation,
                               ecs::res<const Screen> screen,
                               ecs::res<View> view) {
                          if (view->active)
                              return;
                          Vector2 offset = camera.offset;
                          if (camera.centered) {
                              offset.x += screen->width * 0.5f;
                              offset.y += screen->height * 0.5f;
                          }
                          view->camera = Camera2D{
                              .offset = offset,
                              .target = draw_position(position, presentation),
                              .rotation = rotation.value * RAD2DEG,
                              .zoom = camera.zoom,
                          };
                          view->active = true;
                          BeginMode2D(view->camera);
                      });

    const auto draw_sprite = [](const Sprite &sprite,
                                Vector2 at,
                                const GlobalRotation2d &rotation,
                                const GlobalScale2d &scale,
                                ecs::res<const Textures> textures,
                                ecs::res<View> view) {
        if (!view->active) {
            BeginMode2D(view->camera);
            view->active = true;
        }
        const Texture2D &texture = textures->loaded[sprite.texture.id];
        Rectangle source = sprite.source;
        if (source.width == 0 || source.height == 0)
            source = { 0, 0, float(texture.width), float(texture.height) };
        float width = std::fabs(source.width) * scale.x;
        float height = std::fabs(source.height) * scale.y;
        if (sprite.flip_x)
            source.width = -source.width;
        if (sprite.flip_y)
            source.height = -source.height;
        DrawTexturePro(
            texture,
            source,
            { at.x, at.y, width, height },
            { sprite.pivot.x * width, sprite.pivot.y * height },
            rotation.value * RAD2DEG,
            sprite.tint
        );
    };

    auto presented =
        ecs::system("ray.DrawPresentedSprites")
            .phase(EcsOnRender)
            .after(camera)
            .immediate()
            .each([draw_sprite](
                      const Sprite &sprite,
                      const PresentationPosition2d &position,
                      const GlobalRotation2d &rotation,
                      const GlobalScale2d &scale,
                      ecs::res<const Textures> textures,
                      ecs::res<View> view
                  ) {
                draw_sprite(sprite, { position.x, position.y }, rotation, scale, textures, view);
            });
    ecs::set_resource(scene::RenderPipeline{ presented });
    auto sprites =
        ecs::system("ray.DrawSprites")
            .phase(EcsPreRender)
            .manual()
            .immediate()
            .exclude<PresentationPosition2d>()
            .each([draw_sprite](
                      const Sprite &sprite,
                      const GlobalPosition2d &position,
                      const GlobalRotation2d &rotation,
                      const GlobalScale2d &scale,
                      ecs::res<const Textures> textures,
                      ecs::res<View> view
                  ) {
                draw_sprite(sprite, { position.x, position.y }, rotation, scale, textures, view);
            });

    // Scene hooks draw screen-space UI after the sprites of their own layer.
    ecs::system("ray.EndCamera")
        .phase(EcsPreRender)
        .after(sprites)
        .manual()
        .require<scene::Root>()
        .immediate()
        .each([](ecs::res<View> view) {
            if (view->active) {
                EndMode2D();
                view->active = false;
            }
        });

    ecs::system("ray.EndFrame")
        .phase(EcsPostRender)
        .immediate()
        .each([](ecs::res<const View> view) {
            if (view->active)
                EndMode2D();
            EndDrawing();
        });

    ecs::at_fini([] {
        auto &textures = ecs::resource<Textures>();
        for (auto &texture : textures.loaded)
            UnloadTexture(texture);
        textures.loaded.clear();
        textures.by_path.clear();
        CloseWindow();
    });
}

/** Relative paths are tried from the working directory, then from the executable upward. */
static std::string resolve_path(std::string_view path) {
    namespace fs = std::filesystem;
    fs::path relative(path);
    if (relative.is_absolute() || fs::exists(relative))
        return relative.string();
    for (fs::path dir = GetApplicationDirectory(); !dir.empty(); dir = dir.parent_path()) {
        if (fs::exists(dir / relative))
            return (dir / relative).string();
        if (dir == dir.parent_path())
            break;
    }
    return relative.string();
}

Texture load_texture(std::string_view path) {
    auto &textures = ecs::resource<Textures>();
    std::string key(path);
    if (auto it = textures.by_path.find(key); it != textures.by_path.end())
        return Texture{ it->second };
    auto id = static_cast<std::uint32_t>(textures.loaded.size());
    textures.loaded.push_back(LoadTexture(resolve_path(key).c_str()));
    textures.by_path.emplace(std::move(key), id);
    return Texture{ id };
}

Vector2 screen_to_world(Vector2 point) {
    const auto &view = ecs::resource<View>();
    return view.active ? GetScreenToWorld2D(point, view.camera) : point;
}

Vector2 world_to_screen(Vector2 point) {
    const auto &view = ecs::resource<View>();
    return view.active ? GetWorldToScreen2D(point, view.camera) : point;
}

} // namespace ray
