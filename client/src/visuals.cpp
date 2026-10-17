#include "visuals.h"
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
/** Relation: a HUD line describing its target player; it dies with the player. */
struct HudFor {};
/** Tag: the player has its HUD line, even when it arrives already eliminated. */
struct Listed {};

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
} // namespace

void Visuals::import() {
    ecs::relation<HudFor>({ .storage = EcsRelationByTarget, .on_delete_target = EcsDeleteSources });

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
            ecs::entity::create()
                .set(gui::node().at(gui::Bottom, 0, -28), gui::Label{ callsign(slot) })
                .relate<gui::Follows>(player);
        });
    ecs::observe<ecs::OnAdd>().each_for<Eliminated>([](ecs::entity player) {
        player.remove<ray::Sprite, ray::Animation, ray::Animator>();
        // Copy first: tagging moves the labels, which may invalidate the borrowed view.
        const auto view = player.sources<gui::Follows>();
        for (auto label : std::vector<ecs::entity>(view.begin(), view.end()))
            label.add<gui::Hidden>();
    });

    auto list = ecs::system("game.AttachHud")
                    .phase(EcsPostUpdate)
                    .exclude<Listed>()
                    .each([](ecs::entity player, const PlayerSlot &slot) {
                        if (slot.index >= max_players)
                            return;
                        player.add<Listed>();
                        ecs::entity::create()
                            .set(
                                gui::node().at(gui::TopLeft, 16, 16 + 26.0f * slot.index),
                                gui::Label{ callsign(slot) }
                            )
                            .relate<HudFor>(player);
                    });
    ecs::system("game.UpdateHud")
        .phase(EcsPostUpdate)
        .after(list)
        .require_target<HudFor, PlayerSlot, Health>()
        .each([](gui::Label &label, ecs::target<HudFor> player) {
            const auto &health = player.get<Health>();
            auto text = callsign(player.get<PlayerSlot>());
            text += health.current ? std::format("  HP {}/{}", health.current, health.max)
                                   : std::string("  OUT");
            if (player.has<net::Controlled>())
                text += " (you)";
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
