#include "siecs.h"
#include "siecs_spatial.h"
#include <client/raylib/animation.h>
#include <client/raylib/ray.h>
#include <client/scene/scene_manager.h>
#include <client/scene/scenes.h>
#include <raylib.h>
#include <client/ui/ui.h>

struct MenuTag {};

constexpr ray::Animation ShipIdle = {
    .frame = {0, 0, 33, 17},
    .frames = 5,
    .fps = 8,
    .playback = ray::Playback::ping_pong,
};

void menuEnter(ecs::entity root)
{
    root.add<MenuTag>(); // garde le raccourci ENTER de menu.Update

    const auto &screen = ecs::resource<ray::Screen>();
    const float w = 420, h = 460;
    Rectangle box{(screen.width - w) / 2, (screen.height - h) / 2, w, h};

    ui::panel(root, box);
    ui::Stack stack(root, {box.x + 40, box.y + 40, w - 80, h - 80});

    stack.label("MORGANE", 48);
    stack.button("Play", [] { scene::switchTo<scenes::Game>(); });
    stack.button("Options", [] { /* à venir */ });
    stack.button("Quit", [] { ecs::quit(); });
}

void gameEnter(ecs::entity root)
{
    auto ship = ecs::entity::create();
    ship.set(
        Position2d{-100, 0},
        Velocity2d{40, 0},
        ray::Sprite{.texture = ray::load_texture("assets/r-typesheet42.gif")},
        ShipIdle
    );
    ship.child_of(root);
}

scene::ScenesConfig scenes::config()
{
    return {
        .scenes = {
           scene::def<Menu>(menuEnter),
           scene::def<Game>(gameEnter),
        },
        .initial = scene::id<Menu>(),
    };
}

void scenes::register_all()
{
    ecs::component<MenuTag>();

    ecs::system("menu.Update")
        .phase(EcsOnUpdate)
        .require<MenuTag>()
        .each([](ecs::entity, ecs::res<scene::SceneManager> sm) {
            if (IsKeyPressed(KEY_ENTER))
                sm->switchTo<scenes::Game>();
        });
}
