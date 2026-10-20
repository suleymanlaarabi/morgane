#include "effects.h"
#include "movement.h"
#include "preferences.h"
#include "scenes.h"
#include "session.h"
#include "starfield.h"
#include "visuals.h"
#include <client/gui/gui.h>
#include <client/raylib/animation.h>
#include <client/raylib/audio.h>
#include <client/raylib/input.h>
#include <client/raylib/ray.h>
#include <shared/game.h>
#include <shared/scene.h>
#include <siecs.h>
#include <sinet/client.h>
#include <si2d.h>
#include <string_view>

using game::Action;

/** Usage: r-type_client [address[:port]] [--join]. The address fills the menu; --join connects
 * at once instead of waiting on the menu. */
int main(int argc, char **argv) {
    game::Session session;
    bool join = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--join")
            join = true;
        else
            session.address = argv[i];
    }

    ecs::init();
    ecs::import<net::Client>(net::ClientConfig{ .limits = { .timeout_seconds = 5 },
                                                .connect = false });
    ecs::import<ray::Window>(ray::WindowConfig{
        .width = 1920,
        .height = 1080,
        .title = "Morgane",
        .resizable = true,
    });
    ecs::import<gui::Gui>();
    ecs::import<ray::Animations>();
    ecs::import<ray::Audio>();
    ecs::import<ray::Inputs<Action>>(game::default_bindings());
    ecs::import<game::PreferencesModule>();
    game::register_network();
    if (!game::load_level(true)) {
        TraceLog(LOG_ERROR, "failed to load scene: %s", ecs_scene_error() ? ecs_scene_error() : "unknown");
        return 1;
    }
    ecs::import<game::Sessions>(session);
    ecs::import<game::ClientMovement>();
    ecs::import<game::Effects>();
    ecs::import<game::Visuals>();
    ecs::import<game::Starfield>();

    ecs::system("game.PublishInput")
        .phase(EcsPreUpdate)
        .require<net::Controlled>()
        .exclude<game::Eliminated>()
        .each([](ecs::entity player,
                 ecs::res<const ray::Input<Action>> input,
                 ecs::res<const gui::Capture> ui,
                 ecs::res<const game::Session> session) {
            // Keys typed into the UI, or pressed under a menu, never steer the ship.
            const auto playing = ui->keyboard || !session->steering ? ray::Input<Action>{} : *input;
            player.set(
                game::PlayerInput{
                    .move_x = playing.axis(Action::left, Action::right),
                    .move_y = playing.axis(Action::up, Action::down),
                    .fire = playing.down(Action::fire),
                }
            );
        });

    unsigned cameras = 0;
    ecs::query().require<MainCamera2d>().each([&](ecs::entity camera) {
        ++cameras;
        if (!camera.has<ray::Camera>())
            camera.set(ray::Camera{ .zoom = 3 });
    });
    if (!cameras)
        ecs::entity::create().set(Position2d{ 0, 0 }, ray::Camera{ .zoom = 3 });
    scenes::install(join);
    ecs::run();
}
