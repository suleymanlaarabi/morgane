#include "visuals.h"
#include "effects.h"
#include <algorithm>
#include <client/gui/gui.h>
#include <client/raylib/animation.h>
#include <client/raylib/ray.h>
#include <format>
#include <shared/movement.h>
#include <string>
#include <vector>

namespace game {
namespace {
/** Resource: the top-left column holding one HUD line per player. */
struct Hud {
    ecs::entity column;
};
/** Relation: a HUD line, or one of its widgets, describing its target player; it dies with the
 * player. */
struct HudFor {};
/** Tag: the player has its HUD line, even when it arrives already eliminated. */
struct Listed {};
/** Tag: the player has its callsign label. */
struct Named {};
/** Tag: a callsign label following its player. */
struct Callsign {};
/** Tag: the player's callsign is hidden while it is eliminated. */
struct Silenced {};

/** r-typesheet42 stacks one ship colour per row. */
ray::Animation ship_idle(std::uint8_t slot) {
    return {
        .frame = { 0, 17.0f * slot, 33, 17 },
        .frames = 5,
        .fps = 8,
        .playback = ray::Playback::ping_pong,
    };
}
constexpr ray::Animation enemy_spin{ .frame = { 0, 0, 33, 36 }, .frames = 8, .fps = 10 };
constexpr Rectangle enemy_shot_frame{ 135, 5, 9, 8 };
// The next frame is the same shot mirrored, flying left.
constexpr Rectangle player_shot_frame{ 248, 105, 18, 8 };

std::string callsign(const PlayerSlot &slot) { return std::format("P{}", slot.index + 1); }

/** Green while healthy, then yellow, then red on the last point. */
Color health_color(const Health &health) {
    if (health.current * 3 > health.max * 2)
        return { 90, 200, 90, 255 };
    if (health.current > 1)
        return { 230, 190, 60, 255 };
    return { 220, 70, 60, 255 };
}

void set_callsigns_hidden(ecs::entity player, bool hidden) {
    // Copy first: tagging moves the labels, which may invalidate the borrowed view.
    const auto view = player.sources<gui::Follows>();
    for (auto label : std::vector<ecs::entity>(view.begin(), view.end())) {
        if (!label.has<Callsign>())
            continue;
        if (hidden)
            label.add<gui::Hidden>();
        else
            label.remove<gui::Hidden>();
    }
}
} // namespace

void Visuals::import() {
    ecs::relation<HudFor>({ .storage = EcsRelationByTarget, .on_delete_target = EcsDeleteSources });

    // Eliminated players lose their sprite and get it back when a new match starts.
    ecs::system("game.AttachPlayerVisuals")
        .phase(EcsPreRender)
        .immediate()
        .require<ray::PresentationPosition2d>()
        .exclude<ray::Sprite, Eliminated>()
        .each([](ecs::entity player, const PlayerSlot &slot) {
            if (slot.index >= max_players)
                return;
            player.set(
                ray::Sprite{ .texture = ray::load_texture("assets/r-typesheet42.gif") },
                ship_idle(slot.index)
            );
        });
    ecs::system("game.NamePlayers")
        .phase(EcsPostUpdate)
        .require<ray::PresentationPosition2d>()
        .exclude<Named>()
        .each([](ecs::entity player, const PlayerSlot &slot) {
            if (slot.index >= max_players)
                return;
            player.add<Named>();
            ecs::entity::create()
                .set(gui::node().at(gui::Bottom, 0, -28), gui::Label{ callsign(slot) })
                .add<Callsign>()
                .relate<gui::Follows>(player);
        });
    ecs::observe<ecs::OnAdd>().each_for<Eliminated>([](ecs::entity player) {
        player.remove<ray::Sprite, ray::Animation, ray::Animator>();
    });
    // Follows is dense, so callsigns are toggled from their player rather than queried by target.
    ecs::system("game.HideCallsigns")
        .phase(EcsPostUpdate)
        .require<Named, Eliminated>()
        .exclude<Silenced>()
        .each([](ecs::entity player) {
            player.add<Silenced>();
            set_callsigns_hidden(player, true);
        });
    ecs::system("game.ShowCallsigns")
        .phase(EcsPostUpdate)
        .require<Silenced>()
        .exclude<Eliminated>()
        .each([](ecs::entity player) {
            player.remove<Silenced>();
            set_callsigns_hidden(player, false);
        });

    // A column rather than one offset per slot, so the lines follow the text size.
    ecs::set_resource(
        Hud{ ecs::entity::create().set(gui::column(8).at(gui::TopLeft, 16, 16), gui::Bounds{}) }
    );
    // Each line is a health bar followed by the callsign, so the bars line up.
    auto list = ecs::system("game.AttachHud")
                    .phase(EcsPostUpdate)
                    .exclude<Listed>()
                    .each([](ecs::entity player, const PlayerSlot &slot, ecs::res<const Hud> hud) {
                        if (slot.index >= max_players)
                            return;
                        player.add<Listed>();
                        auto line = ecs::entity::create()
                                        .set(gui::row(12).align(gui::Align::Center), gui::Bounds{})
                                        .relate<HudFor>(player)
                                        .child_of(hud->column);
                        ecs::entity::create()
                            .set(gui::node().width(240), gui::ProgressBar{})
                            .relate<HudFor>(player)
                            .child_of(line);
                        ecs::entity::create()
                            .set(gui::Label{ callsign(slot) })
                            .relate<HudFor>(player)
                            .child_of(line);
                    });
    ecs::system("game.UpdateHudBars")
        .phase(EcsPostUpdate)
        .after(list)
        .require_target<HudFor, Health>()
        .each([](gui::ProgressBar &bar, ecs::target<HudFor> player) {
            const auto &health = player.get<Health>();
            bar.value = health.current;
            bar.max = std::max<float>(health.max, 1);
            bar.fill = health_color(health);
        });
    ecs::system("game.UpdateHudLabels")
        .phase(EcsPostUpdate)
        .after(list)
        .require_target<HudFor, PlayerSlot, Health>()
        .each([](gui::Label &label, ecs::target<HudFor> player) {
            auto text = callsign(player.get<PlayerSlot>());
            if (player.has<net::Controlled>())
                text += " (you)";
            if (!player.get<Health>().current)
                text += "  ELIMINATED";
            if (label.text != text)
                label.text = std::move(text);
        });

    ecs::system("game.AttachKindVisuals")
        .phase(EcsPreRender)
        .immediate()
        .exclude<ray::Sprite>()
        .each([](ecs::entity body, const EntityKind &kind) {
            // Kinds outside the schema come from the network and get no sprite.
            switch (Kind(kind.value)) {
            case Kind::enemy:
                body.set(
                    ray::Sprite{ .texture = ray::load_texture("assets/r-typesheet5.gif") },
                    enemy_spin
                );
                break;
            case Kind::enemy_shot:
                body.set(
                    ray::Sprite{ .texture = ray::load_texture("assets/r-typesheet43.gif"),
                                 .source = enemy_shot_frame }
                );
                break;
            case Kind::player_shot:
                body.set(
                    ray::Sprite{ .texture = ray::load_texture("assets/r-typesheet1.gif"),
                                 .source = player_shot_frame }
                );
                break;
            }
        });

    ecs::set_resource(MatchView{});
    ecs::observe<net::OnConnect>().each([](ecs::entity, ecs::res<MatchView> view) { *view = {}; });
    ecs::system("game.ViewMatch")
        .phase(EcsPostUpdate)
        .each([](const Match &match, ecs::res<MatchView> view, ecs::res<const Sfx> sfx) {
            const auto over = std::uint8_t(Phase::over);
            if (match.phase == over && view->match.phase != over)
                ray::play(sfx->game_over);
            if (match.phase != over)
                view->voted = false;
            view->match = match;
        });
    ecs::system("game.ViewLocalPlayer")
        .phase(EcsPostUpdate)
        .require<net::Controlled>()
        .each([](const Health &health, ecs::res<MatchView> view) {
            view->eliminated = health.current == 0;
        });

    // Show the whole playfield at the largest zoom the window allows.
    ecs::system("game.FitCamera")
        .phase(EcsPostUpdate)
        .each([](ray::Camera &camera, ecs::res<const ray::Screen> screen) {
            camera.zoom = std::min(
                float(screen->width) / (2 * play_half_width),
                float(screen->height) / (2 * play_half_height)
            );
        });
}

} // namespace game
