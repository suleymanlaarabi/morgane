#include "scenes.h"
#include <client/gui/gui.h>
#include <raylib.h>

namespace scenes {

namespace {
struct Play {};
struct Quit {};
struct Resume {};
struct BackToMenu {};

/** Centered titled panel whose buttons all share its width. */
ecs::entity dialog(ecs::entity panel, const char *title) {
    return panel.set(
        gui::column(12).center().width(360).pad(24).align(gui::Align::Stretch),
        gui::Panel{ title }
    );
}
} // namespace

void install() {
    auto menu = scene::define<Menu>()
                    .on_enter([](scene::Context<Menu> ctx) {
                        auto box = dialog(ctx.spawn(), "MORGANE");
                        ctx.spawn(box).set(gui::Label{ "Entree: jouer" });
                        ctx.spawn(box).set(gui::Button{ "Jouer" }).add<Play>();
                        ctx.spawn(box).set(gui::Button{ "Quitter" }).add<Quit>();
                    })
                    .on_update([](Commands scenes) {
                        if (IsKeyPressed(KEY_ENTER))
                            scenes.replace<Game>({ .level = 1 });
                    });

    auto game = scene::define<Game>()
                    .on_enter([](scene::Context<Game> ctx) {
                        // The top-left corner holds the player HUD.
                        ctx.spawn().set(
                            gui::node().at(gui::BottomLeft, 24, -24),
                            gui::Label{ "Echap: pause" }
                        );
                    })
                    .on_update([](Commands scenes) {
                        if (IsKeyPressed(KEY_ESCAPE))
                            scenes.push<Pause>();
                    });

    auto pause =
        scene::define<Pause>()
            .on_enter([](scene::Context<Pause> ctx) {
                auto box = dialog(ctx.spawn(), "PAUSE");
                ctx.spawn(box).set(gui::Button{ "Reprendre" }).add<Resume>();
                ctx.spawn(box).set(gui::Button{ "Menu principal" }).add<BackToMenu>();
            })
            .on_update([](Commands scenes) {
                if (IsKeyPressed(KEY_ESCAPE))
                    scenes.pop();
                else if (IsKeyPressed(KEY_M))
                    scenes.reset<Menu>();
            })
            .on_render([] {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.65f));
            });

    gui::on_click<Play>([](Commands scenes) { scenes.replace<Game>({ .level = 1 }); });
    gui::on_click<Quit>([] { ecs::quit(); });
    gui::on_click<Resume>([](Commands scenes) { scenes.pop(); });
    gui::on_click<BackToMenu>([](Commands scenes) { scenes.reset<Menu>(); });

    ecs::import<App>(std::move(menu), std::move(game), std::move(pause));
    App::start<Menu>();
}

} // namespace scenes
