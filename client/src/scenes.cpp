#include "scenes.h"
#include "preferences.h"
#include "session.h"
#include "visuals.h"
#include <algorithm>
#include <client/gui/gui.h>
#include <format>
#include <raylib.h>
#include <sinet/client.h>

namespace scenes {

namespace {
using game::Action;

// Widget roles, shared by every scene that shows the widget.
struct AddressField {};
struct Join {};
struct OpenOptions {};
struct Quit {};
struct Cancel {};
struct Resume {};
struct Leave {};
struct WaveLabel {};
struct ControlsLabel {};
struct SpectatorBanner {};
struct GameOverPanel {};
struct ReadyLabel {};
struct Vote {};
struct VolumeSlider {};
struct FlashesBox {};
struct LargeTextBox {};
struct Rebind {
    Action action;
};
struct ResetBindings {};
struct Back {};

/** Resource: the action waiting for its new key in the options, or none. */
struct Rebinding {
    int action = -1;
};

constexpr const char *action_labels[] = { "Up", "Down", "Left", "Right", "Fire" };
static_assert(std::size(action_labels) == std::size_t(Action::count));

/** Centered titled panel, as wide as its widest child, whose buttons all share its width. */
ecs::entity dialog(ecs::entity panel, const char *title) {
    return panel.set(
        gui::column(12).center().pad(24).align(gui::Align::Stretch),
        gui::Panel{ title }
    );
}

void start(Commands scenes, game::Session &session) {
    if (game::join(session))
        scenes.replace<Connecting>();
    else
        scenes.reset<Menu>();
}

void leave(Commands scenes) {
    net::disconnect();
    scenes.reset<Menu>();
}

void vote(game::MatchView &view, const net::LocalPeer &peer) {
    if (!peer.connection || view.match.phase != std::uint8_t(game::Phase::over))
        return;
    view.voted = !view.voted;
    const game::RestartVote ready{ view.voted };
    ecs::trigger<game::RestartVote>(peer.connection, &ready);
}

std::string controls(const ray::Bindings<Action> &bindings) {
    return std::format(
        "Up: {}   Down: {}   Left: {}   Right: {}   Fire: {}   Pause: Esc",
        game::action_keys(bindings, Action::up),
        game::action_keys(bindings, Action::down),
        game::action_keys(bindings, Action::left),
        game::action_keys(bindings, Action::right),
        game::action_keys(bindings, Action::fire)
    );
}

/** Two systems keep a tagged widget shown exactly while `visible` holds. */
template <typename Tag, typename Scene, typename Visible>
void show_when(
    scene::Definition<Scene> &definition,
    const char *show,
    const char *hide,
    Visible visible
) {
    definition.system(show).template require<Tag, gui::Hidden>().each(
        [visible](ecs::entity widget, ecs::res<const game::MatchView> view) {
            if (visible(*view))
                widget.remove<gui::Hidden>();
        }
    );
    definition.system(hide).template require<Tag>().template exclude<gui::Hidden>().each(
        [visible](ecs::entity widget, ecs::res<const game::MatchView> view) {
            if (!visible(*view))
                widget.add<gui::Hidden>();
        }
    );
}
} // namespace

void install(bool join) {
    ecs::set_resource(Rebinding{});

    auto menu = scene::define<Menu>().on_enter([](scene::Context<Menu> ctx) {
        const auto &session = ecs::resource<game::Session>();
        auto box = dialog(ctx.spawn(), "MORGANE");
        ctx.spawn(box).set(gui::Label{ "Server address (host:port)" });
        auto field = ctx.spawn(box).set(gui::TextBox{ session.address, 64 }).add<AddressField>();
        ctx.spawn(box).set(gui::Button{ "Join" }).add<Join>();
        ctx.spawn(box).set(gui::Button{ "Options" }).add<OpenOptions>();
        ctx.spawn(box).set(gui::Button{ "Quit" }).add<Quit>();
        if (!session.error.empty())
            ctx.spawn(box).set(
                gui::Label{ "Error: " + session.error },
                gui::Style{ .text = Color{ 255, 140, 140, 255 } }
            );
        ctx.spawn(box).set(gui::Label{ "Enter: join" });
        // Typing goes straight to the address, and Enter joins.
        ecs::resource<gui::Focus>().entity = field;
    });

    auto connecting =
        scene::define<Connecting>()
            .on_enter([](scene::Context<Connecting> ctx) {
                auto box = dialog(ctx.spawn(), "CONNECTING");
                ctx.spawn(box).set(
                    gui::Label{
                        std::format("Connecting to {}...", ecs::resource<game::Session>().address) }
                );
                ctx.spawn(box).set(gui::Button{ "Cancel" }).add<Cancel>();
                ctx.spawn(box).set(gui::Label{ "Esc: cancel" });
            })
            .on_update([](Commands scenes, ecs::res<const game::Session> session) {
                if (session->connected)
                    scenes.replace<Game>({ .level = 1 });
                else if (!session->connecting || IsKeyPressed(KEY_ESCAPE))
                    leave(scenes);
            });

    auto game =
        scene::define<Game>()
            .on_enter([](scene::Context<Game> ctx) {
                auto &session = ecs::resource<game::Session>();
                session.in_game = session.steering = true;
                // The top-left corner holds the player HUD.
                ctx.spawn()
                    .set(gui::node().at(gui::TopRight, -24, 16), gui::Label{})
                    .add<WaveLabel>();
                ctx.spawn()
                    .set(gui::node().at(gui::BottomLeft, 24, -24), gui::Label{})
                    .add<ControlsLabel>();
                ctx.spawn()
                    .set(
                        gui::node().at(gui::Bottom, 0, -72),
                        gui::Label{ "You are eliminated: watching your teammates." }
                    )
                    .add<SpectatorBanner, gui::Hidden>();
                auto box = dialog(ctx.spawn(), "GAME OVER").add<GameOverPanel, gui::Hidden>();
                ctx.spawn(box).set(gui::Label{ "All ships have been destroyed." });
                ctx.spawn(box).set(gui::Label{}).add<ReadyLabel>();
                ctx.spawn(box).set(gui::Button{ "Play again" }).add<Vote>();
                ctx.spawn(box).set(gui::Button{ "Leave match" }).add<Leave>();
                ctx.spawn(box).set(gui::Label{ "Enter: play again   M: menu" });
            })
            .on_pause([](scene::Context<Game>) { ecs::resource<game::Session>().steering = false; })
            .on_resume([](scene::Context<Game>) { ecs::resource<game::Session>().steering = true; })
            .on_exit([](scene::Context<Game>) {
                auto &session = ecs::resource<game::Session>();
                session.in_game = session.steering = false;
            })
            .on_update([](Commands scenes,
                          ecs::res<game::MatchView> view,
                          ecs::res<const net::LocalPeer> peer) {
                if (IsKeyPressed(KEY_ESCAPE))
                    scenes.push<Pause>();
                else if (view->match.phase == std::uint8_t(game::Phase::over)) {
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
                        vote(*view, *peer);
                    else if (IsKeyPressed(KEY_M))
                        leave(scenes);
                }
            });
    game.system("game.ShowWave")
        .require<WaveLabel>()
        .each([](gui::Label &label, ecs::res<const game::MatchView> view) {
            const auto &match = view->match;
            auto text = match.phase == std::uint8_t(game::Phase::waiting)
                            ? std::string("Waiting for players")
                            : std::format(
                                  "Wave {}   Players {}/{}",
                                  std::max(match.wave, 1u),
                                  match.players,
                                  game::max_players
                              );
            if (label.text != text)
                label.text = std::move(text);
        });
    game.system("game.ShowControls")
        .require<ControlsLabel>()
        .each([](gui::Label &label, ecs::res<const ray::Bindings<Action>> bindings) {
            if (auto text = controls(*bindings); label.text != text)
                label.text = std::move(text);
        });
    show_when<GameOverPanel>(game, "game.ShowGameOver", "game.HideGameOver", [](const auto &view) {
        return view.match.phase == std::uint8_t(game::Phase::over);
    });
    show_when<SpectatorBanner>(
        game,
        "game.ShowSpectator",
        "game.HideSpectator",
        [](const auto &view) {
            return view.eliminated && view.match.phase == std::uint8_t(game::Phase::playing);
        }
    );
    game.system("game.ShowVotes")
        .require<ReadyLabel>()
        .each([](gui::Label &label, ecs::res<const game::MatchView> view) {
            auto text =
                std::format("Ready to replay: {}/{}", view->match.ready, view->match.players);
            if (label.text != text)
                label.text = std::move(text);
        });
    game.system("game.ShowVote")
        .require<Vote>()
        .each([](gui::Button &button, ecs::res<const game::MatchView> view) {
            const char *text = view->voted ? "Cancel vote" : "Play again";
            if (button.text != text)
                button.text = text;
        });

    auto pause =
        scene::define<Pause>()
            .on_enter([](scene::Context<Pause> ctx) {
                auto box = dialog(ctx.spawn(), "PAUSE");
                ctx.spawn(box).set(gui::Label{ "The match goes on for the other players." });
                ctx.spawn(box).set(gui::Button{ "Resume" }).add<Resume>();
                ctx.spawn(box).set(gui::Button{ "Options" }).add<OpenOptions>();
                ctx.spawn(box).set(gui::Button{ "Leave match" }).add<Leave>();
                ctx.spawn(box).set(gui::Label{ "Esc: resume   M: menu" });
            })
            .on_update([](Commands scenes) {
                if (IsKeyPressed(KEY_ESCAPE))
                    scenes.pop();
                else if (IsKeyPressed(KEY_M))
                    leave(scenes);
            })
            .on_render([] {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.65f));
            });

    auto options =
        scene::define<Options>()
            .on_enter([](scene::Context<Options> ctx) {
                const auto &preferences = ecs::resource<game::Preferences>();
                auto box = dialog(ctx.spawn(), "OPTIONS");
                ctx.spawn(box)
                    .set(
                        gui::Slider{ .label = "Volume",
                                     .value = preferences.volume,
                                     .bar = true,
                                     .show_value = true }
                    )
                    .add<VolumeSlider>();
                ctx.spawn(box)
                    .set(
                        gui::CheckBox{ "Blink hit ships",
                                       preferences.flashes }
                    )
                    .add<FlashesBox>();
                ctx.spawn(box)
                    .set(gui::CheckBox{ "Large text", preferences.large_text })
                    .add<LargeTextBox>();
                ctx.spawn(box).set(gui::Line{ "Controls" });
                for (std::size_t action = 0; action < std::size(action_labels); ++action) {
                    auto row =
                        ctx.spawn(box).set(gui::row(12).align(gui::Align::Center), gui::Bounds{});
                    ctx.spawn(row).set(gui::node().width(160), gui::Label{ action_labels[action] });
                    ctx.spawn(row).set(gui::node().grow(), gui::Button{}, Rebind{ Action(action) });
                }
                ctx.spawn(box).set(
                    gui::Label{ "Click a control, then press its key." }
                );
                ctx.spawn(box).set(gui::Button{ "Reset to defaults" }).add<ResetBindings>();
                ctx.spawn(box).set(gui::Button{ "Back" }).add<Back>();
                ctx.spawn(box).set(gui::Label{ "Esc: back" });
            })
            .on_exit([](scene::Context<Options>) {
                ecs::resource<Rebinding>() = {};
                game::save_preferences();
            })
            .on_update([](Commands scenes,
                          ecs::res<Rebinding> rebinding,
                          ecs::res<game::Preferences> preferences) {
                if (rebinding->action < 0) {
                    if (IsKeyPressed(KEY_ESCAPE))
                        scenes.pop();
                    return;
                }
                const int key = GetKeyPressed();
                if (!key)
                    return;
                if (key != KEY_ESCAPE) {
                    preferences->bindings.bind(Action(rebinding->action), { KeyboardKey(key) });
                    game::apply_preferences();
                }
                rebinding->action = -1;
            });
    options.system("game.ShowBindings")
        .each([](gui::Button &button,
                 const Rebind &rebind,
                 ecs::res<const game::Preferences> preferences,
                 ecs::res<const Rebinding> rebinding) {
            auto text = rebinding->action == int(rebind.action)
                            ? std::string("Press a key (Esc: cancel)")
                            : game::action_keys(preferences->bindings, rebind.action);
            if (button.text != text)
                button.text = std::move(text);
        });

    gui::on_submit<AddressField>([](Commands scenes, ecs::res<game::Session> session) {
        start(scenes, *session);
    });
    gui::on_change<AddressField>([](const gui::TextBox &box, ecs::res<game::Session> session) {
        session->address = box.text;
    });
    gui::on_click<Join>([](Commands scenes, ecs::res<game::Session> session) {
        start(scenes, *session);
    });
    gui::on_click<OpenOptions>([](Commands scenes) { scenes.push<Options>(); });
    gui::on_click<Quit>([] { ecs::quit(); });
    gui::on_click<Cancel>([](Commands scenes) { leave(scenes); });
    gui::on_click<Resume>([](Commands scenes) { scenes.pop(); });
    gui::on_click<Leave>([](Commands scenes) { leave(scenes); });
    gui::on_click<Vote>([](ecs::res<game::MatchView> view, ecs::res<const net::LocalPeer> peer) {
        vote(*view, *peer);
    });
    gui::on_change<VolumeSlider>([](const gui::Slider &slider,
                                    ecs::res<game::Preferences> preferences) {
        preferences->volume = slider.value;
        game::apply_preferences();
    });
    gui::on_change<FlashesBox>([](const gui::CheckBox &box,
                                  ecs::res<game::Preferences> preferences) {
        preferences->flashes = box.checked;
    });
    gui::on_change<LargeTextBox>([](const gui::CheckBox &box,
                                    ecs::res<game::Preferences> preferences) {
        preferences->large_text = box.checked;
        game::apply_preferences();
    });
    gui::on_click<Rebind>([](const Rebind &rebind, ecs::res<Rebinding> rebinding) {
        rebinding->action = int(rebind.action);
    });
    gui::on_click<ResetBindings>([](ecs::res<game::Preferences> preferences) {
        preferences->bindings = game::default_bindings();
        game::apply_preferences();
    });
    gui::on_click<Back>([](Commands scenes) { scenes.pop(); });

    // A lost connection ends the match from any scene stacked over it.
    ecs::system("game.LeaveLostMatch")
        .phase(EcsOnUpdate)
        .each([](Commands scenes, ecs::res<game::Session> session) {
            if (session->in_game && !session->connected && !session->connecting) {
                session->in_game = false;
                scenes.reset<Menu>();
            }
        });

    ecs::import<App>(
        std::move(menu),
        std::move(connecting),
        std::move(game),
        std::move(pause),
        std::move(options)
    );
    App::start<Menu>();
    if (join && game::join(ecs::resource<game::Session>()))
        Commands{ &App::runtime() }.replace<Connecting>();
}

} // namespace scenes
