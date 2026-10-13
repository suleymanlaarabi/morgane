#include "net/include/sinet/net.h"
#include <client.h>
#include <client/raylib/animation.h>
#include <client/raylib/input.h>
#include <client/raylib/ray.h>
#include <shared/game.h>
#include <sinet/client.h>

using game::Action;

int main() {
    ecs::init();
    ecs::import<net::Client>(net::ClientConfig{
        .address = "127.0.0.1",
        .port = 4242,
    });
    ecs::import<ray::Window>(ray::WindowConfig{
        .width = 1920,
        .height = 1080,
        .title = "Morgane",
    });
    ecs::import<ray::Animations>();
    ecs::import<ray::Inputs<Action>>(ray::Bindings<Action>{}
                                         .bind(Action::up, { KEY_UP, KEY_W })
                                         .bind(Action::down, { KEY_DOWN, KEY_S })
                                         .bind(Action::left, { KEY_LEFT, KEY_A })
                                         .bind(Action::right, { KEY_RIGHT, KEY_D })
                                         .bind(Action::fire, { KEY_SPACE }));
    game::register_network();

    ecs::system("game.PublishInput")
        .phase(EcsPreUpdate)
        .require<net::Controlled>()
        .each([](ecs::entity player, ecs::res<const ray::Input<Action>> input) {
            player.set(game::PlayerInput{
                .move_x = input->axis(Action::left, Action::right),
                .move_y = input->axis(Action::up, Action::down),
                .fire = input->down(Action::fire),
            });
        });

    constexpr ray::Animation ShipIdle = {
        .frame = { 0, 0, 33, 17 },
        .frames = 5,
        .fps = 8,
        .playback = ray::Playback::ping_pong,
    };

    ecs::entity::create().set(Position2d{ 0, 0 }, ray::Camera{ .zoom = 3 });
    ecs::entity::create().set(
        Position2d{ -100, 0 },
        Velocity2d{ 40, 0 },
        ray::Sprite{ .texture = ray::load_texture("assets/r-typesheet42.gif") },
        ShipIdle
    );
    ecs::run();
}
