#include "siecs.h"
#include "siecs_spatial.h"
#include <client/raylib/animation.h>
#include <client/raylib/ray.h>
#include <client/scene/scene_manager.h>
#include <client/scene/scenes.h>
#include <raylib.h>

struct MenuTag {};

constexpr ray::Animation ShipIdle = {
    .frame = {0, 0, 33, 17},
    .frames = 5,
    .fps = 8,
    .playback = ray::Playback::ping_pong,
};

void menuEnter(ecs::entity root)
{
    auto e = ecs::entity::create();
    e.add<MenuTag>();
    e.child_of(root);
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
