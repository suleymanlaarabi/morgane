#include "client/scene/scene_manager.h"
#include "client/scene/scenes.h"
#include "movement.h"
#include "siecs_spatial.h"
#include "starfield.h"
#include "visuals.h"
#include <client/gui/gui.h>
#include <client/raylib/animation.h>
#include <client/raylib/input.h>
#include <client/raylib/ray.h>
#include <shared/game.h>
#include <siecs.h>
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
    ecs::import<gui::Gui>(gui::Theme{ .font_size = 20, .style = "dark" });
    ecs::import<ray::Animations>();
    ecs::import<ray::Inputs<Action>>(ray::Bindings<Action>{}
                                         .bind(Action::up, { KEY_UP, KEY_W })
                                         .bind(Action::down, { KEY_DOWN, KEY_S })
                                         .bind(Action::left, { KEY_LEFT, KEY_A })
                                         .bind(Action::right, { KEY_RIGHT, KEY_D })
                                         .bind(Action::fire, { KEY_SPACE }));
    game::register_network();
    ecs::import<game::ClientMovement>();
    ecs::import<game::Visuals>();
    ecs::import<game::Starfield>();

    ecs::system("game.PublishInput")
        .phase(EcsPreUpdate)
        .require<net::Controlled>()
        .exclude<game::Eliminated>()
        .each([](ecs::entity player,
                 ecs::res<const ray::Input<Action>> input,
                 ecs::res<const gui::Capture> ui) {
            // Keys typed into the UI never steer the ship.
            const auto playing = ui->keyboard ? ray::Input<Action>{} : *input;
            player.set(
                game::PlayerInput{
                    .move_x = playing.axis(Action::left, Action::right),
                    .move_y = playing.axis(Action::up, Action::down),
                    .fire = playing.down(Action::fire),
                }
            );
        });

    ecs::entity::create().set(Position2d{ 0, 0 }, ray::Camera{ .zoom = 3 });
    scenes::install();
    ecs::run();
}
