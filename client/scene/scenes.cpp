#include "scenes.h"
#include <client/raylib/animation.h>
#include <client/raylib/ray.h>
#include <raylib.h>
#include <siecs_spatial.h>

namespace scenes {

void install() {
    auto menu = scene::define<Menu>()
                    .on_update([](Commands scenes) {
                        if (IsKeyPressed(KEY_ENTER))
                            scenes.replace<Game>({ .level = 1 });
                    })
                    .on_render([] {
                        DrawText("MORGANE", 60, 60, 40, RAYWHITE);
                        DrawText("Press Enter to play", 60, 120, 24, LIGHTGRAY);
                    });

    auto game = scene::define<Game>()
                    .on_enter([](scene::Context<Game> ctx) {
                        ctx.spawn().set(
                            Position2d{ -100, 0 },
                            Velocity2d{ 40, 0 },
                            ray::Sprite{ .texture = ray::load_texture("assets/r-typesheet42.gif") },
                            ShipIdle
                        );
                    })
                    .on_update([](Commands scenes) {
                        if (IsKeyPressed(KEY_ESCAPE))
                            scenes.push<Pause>();
                    })
                    .on_render([] { DrawText("Esc: pause", 30, 30, 24, LIGHTGRAY); });

    auto pause =
        scene::define<Pause>()
            .on_update([](Commands scenes) {
                if (IsKeyPressed(KEY_ESCAPE))
                    scenes.pop();
                else if (IsKeyPressed(KEY_M))
                    scenes.reset<Menu>();
            })
            .on_render([] {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.65f));
                DrawText("PAUSED", 60, 60, 40, RAYWHITE);
                DrawText("Esc: resume   M: menu", 60, 120, 24, LIGHTGRAY);
            });

    ecs::import<App>(std::move(menu), std::move(game), std::move(pause));
    App::start<Menu>();
}

} // namespace scenes
