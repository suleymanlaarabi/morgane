#include "starfield.h"
#include <client/raylib/ray.h>
#include <client/scene/scenes.h>
#include <cmath>
#include <random>

namespace game {
namespace {
/** Screen-space star: position in [0, 1) of the screen, speed in screen widths per second. */
struct Star {
    float x, y, speed, size;
    unsigned char shade;
};
constexpr int star_count = 120;
} // namespace

void Starfield::import() {
    // Stars belong to the Game instance, so they die with it.
    ecs::observe<ecs::OnAdd>().each_for<scenes::Game>([](ecs::entity root) {
        const scene::Context<scenes::Game> ctx{ root };
        std::mt19937 random{ 42 }; // same sky every game
        std::uniform_real_distribution<float> unit{ 0, 1 };
        for (int i = 0; i < star_count; ++i) {
            const float depth = unit(random); // 0 = far, 1 = near
            ctx.spawn().set(
                Star{
                    .x = unit(random),
                    .y = unit(random),
                    .speed = 0.01f + 0.07f * depth * depth,
                    .size = depth < 0.8f ? 2.0f : 3.0f,
                    .shade = static_cast<unsigned char>(70 + 185 * depth),
                }
            );
        }
    });
    ecs::system("game.ScrollStars")
        .phase(EcsOnUpdate)
        .exclude_up<ecs::ChildOf, SimulationPaused>()
        .each([](Star &star, ecs::res<const DeltaTime> dt) {
            star.x -= star.speed * dt->value;
            star.x -= std::floor(star.x);
        });
    // After BeginDrawing and before the world camera, so every sprite covers the sky.
    const auto backdrop = ecs::phase("game.Backdrop").after(EcsPreRender).before(EcsOnRender);
    ecs::system("game.DrawStars")
        .phase(backdrop.id())
        .immediate()
        .each([](const Star &star, ecs::res<const ray::Screen> screen) {
            const auto shade = star.shade;
            DrawRectangleV(
                { star.x * screen->width, star.y * screen->height },
                { star.size, star.size },
                Color{ shade, shade, shade, 255 }
            );
        });
}

} // namespace game
