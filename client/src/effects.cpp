#include "effects.h"
#include "movement.h"
#include "preferences.h"
#include <client/raylib/animation.h>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

namespace game {
namespace {
constexpr int sample_rate = 22050;
constexpr float tau = 2 * std::numbers::pi_v<float>;

/** Square wave sliding from `from` to `to` Hz, fading out over `seconds`. */
std::vector<float> sweep(float from, float to, float seconds, float gain) {
    std::vector<float> samples(std::size_t(seconds * sample_rate));
    float phase = 0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const float t = float(i) / float(samples.size());
        phase += (from + (to - from) * t) / sample_rate;
        samples[i] = (std::fmod(phase, 1.0f) < 0.5f ? gain : -gain) * (1 - t);
    }
    return samples;
}

/** Low-passed noise; a lower `smoothing` rumbles deeper. */
std::vector<float> noise(float seconds, float smoothing, float gain) {
    std::vector<float> samples(std::size_t(seconds * sample_rate));
    std::minstd_rand random{ 7 };
    std::uniform_real_distribution<float> white{ -1, 1 };
    float low = 0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const float t = float(i) / float(samples.size());
        low += smoothing * (white(random) - low);
        samples[i] = low * gain * (1 - t) * (1 - t);
    }
    return samples;
}

/** Sine notes played one after the other. */
std::vector<float> melody(std::initializer_list<float> notes, float seconds, float gain) {
    std::vector<float> samples;
    const auto length = std::size_t(seconds * sample_rate);
    for (float note : notes)
        for (std::size_t i = 0; i < length; ++i) {
            const float t = float(i) / float(length);
            samples.push_back(
                std::sin(tau * note * float(i) / sample_rate) * gain * (1 - t * 0.7f)
            );
        }
    return samples;
}

std::vector<float> mix(std::vector<float> a, const std::vector<float> &b) {
    a.resize(std::max(a.size(), b.size()));
    for (std::size_t i = 0; i < b.size(); ++i)
        a[i] += b[i];
    return a;
}

/** Last Health seen, to tell a hit from a heal or the first snapshot. */
struct SeenHealth {
    std::uint8_t current = UINT8_MAX;
};
/** Seconds left of the hit flash, matching the server's invulnerability window. */
struct Flash {
    float remaining = 1;
};
/** A destroyed body's stand-in, kept on its trajectory until the presented time reaches the
 * destruction, so the explosion appears where the body is drawn. */
struct Doomed {
    std::uint64_t tick = 0;
    Kind kind{};
    float fallback = 0.5f; // seconds, in case the timeline stops
};
/** Tag: an explosion, removed when its clip ends. */
struct Blast {};

constexpr ray::Animation explosion{ .frame = { 129, 1, 33, 32 },
                                    .frames = 5,
                                    .fps = 14,
                                    .playback = ray::Playback::once };
constexpr ray::Animation spark{ .frame = { 294, 1, 16, 32 },
                                .frames = 4,
                                .fps = 20,
                                .playback = ray::Playback::once };

void blast(Position2d at, const ray::Animation &clip, float scale) {
    ecs::entity::create()
        .set(
            at,
            Scale2d{ scale, scale },
            ray::Sprite{ .texture = ray::load_texture("assets/r-typesheet44.gif") },
            clip
        )
        // Static computes the global transform at once, so blasts spawned after propagation
        // are still drawn at their place and scale from their first frame.
        .add<Blast, Static>();
}
} // namespace

void Effects::import() {
    ecs::set_resource(
        Sfx{
            .shot = ray::make_sound(sweep(1400, 500, 0.08f, 0.18f), sample_rate),
            .explosion = ray::make_sound(noise(0.45f, 0.08f, 0.9f), sample_rate),
            .hit = ray::make_sound(
                mix(sweep(220, 90, 0.2f, 0.3f), noise(0.2f, 0.3f, 0.4f)),
                sample_rate
            ),
            .eliminated = ray::make_sound(
                mix(noise(0.9f, 0.04f, 1.0f), sweep(300, 40, 0.6f, 0.25f)),
                sample_rate
            ),
            .game_over = ray::make_sound(melody({ 440, 349, 262, 196 }, 0.28f, 0.35f), sample_rate),
        }
    );
    ecs::component<Health>().with<SeenHealth>();

    ecs::observe<ecs::OnSet>().each_for<EntityKind>(
        [](ecs::entity, ecs::observer_event event, ecs::res<const Sfx> sfx) {
            if (Kind(event.trigger_data<EntityKind>()->value) == Kind::player_shot)
                ray::play(sfx->shot, 0.5f);
        }
    );
    ecs::observe<Destroyed>().each([](ecs::entity body,
                                      ecs::observer_event event,
                                      const Trajectory &trajectory,
                                      const EntityKind &kind) {
        auto ghost = ecs::entity::create().set(
            trajectory,
            Doomed{ event.trigger_data<Destroyed>()->tick, Kind(kind.value) }
        );
        if (const auto *sprite = body.try_get<ray::Sprite>())
            ghost.set(*sprite);
    });
    ecs::system("game.Detonate")
        .phase(EcsPostUpdate)
        .each([](ecs::entity ghost,
                 Doomed &doomed,
                 const ray::PresentationPosition2d &at,
                 ecs::res<const ServerTimeline> timeline,
                 ecs::res<const DeltaTime> dt,
                 ecs::res<const Sfx> sfx) {
            doomed.fallback -= dt->value;
            if (timeline->cursor < double(doomed.tick) && doomed.fallback > 0)
                return;
            if (doomed.kind == Kind::enemy) {
                blast({ at.x, at.y }, explosion, 1);
                ray::play(sfx->explosion, 0.8f);
            } else {
                blast({ at.x, at.y }, spark, 0.6f);
            }
            ghost.kill();
        });
    ecs::observe<ray::AnimationFinished>().each([](ecs::entity effect, const Blast &) {
        effect.kill();
    });

    ecs::system("game.NoticeDamage")
        .phase(EcsPostUpdate)
        .each([](ecs::entity body,
                 const Health &health,
                 SeenHealth &seen,
                 ecs::optional<const ray::PresentationPosition2d> at,
                 ecs::res<const Sfx> sfx) {
            const bool hit = seen.current != UINT8_MAX && health.current < seen.current;
            seen.current = health.current;
            if (!hit)
                return;
            if (health.current) {
                body.set(Flash{});
                ray::play(sfx->hit);
            } else {
                if (at)
                    blast({ at->x, at->y }, explosion, 2);
                ray::play(sfx->eliminated);
            }
        });
    // At most three blinks per second, and none when flashes are turned off.
    ecs::system("game.FlashHits")
        .phase(EcsPostUpdate)
        .each([](ecs::entity body,
                 Flash &flash,
                 ray::Sprite &sprite,
                 ecs::res<const DeltaTime> dt,
                 ecs::res<const Preferences> preferences) {
            flash.remaining -= dt->value;
            const bool lit = !preferences->flashes || std::fmod(flash.remaining, 0.34f) > 0.17f;
            sprite.tint = flash.remaining > 0 && lit ? Color{ 255, 80, 80, 255 } : WHITE;
            if (flash.remaining <= 0)
                body.remove<Flash>();
        });
}

} // namespace game
