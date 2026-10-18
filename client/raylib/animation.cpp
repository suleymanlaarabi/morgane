#include "animation.h"
#include <algorithm>

namespace ray {

static std::uint16_t frame_index(const Animation &clip, float time) {
    int frames = std::max<int>(clip.frames, 1);
    int step = static_cast<int>(time * clip.fps);
    switch (clip.playback) {
    case Playback::loop:
        return step % frames;
    case Playback::once:
        return std::min(step, frames - 1);
    case Playback::ping_pong: {
        if (frames == 1)
            return 0;
        int period = 2 * frames - 2;
        int p = step % period;
        return p < frames ? p : period - p;
    }
    }
    return 0;
}

static Rectangle frame_source(const Animation &clip, std::uint16_t index) {
    int columns = clip.columns ? clip.columns : std::max<int>(clip.frames, 1);
    Rectangle source = clip.frame;
    source.x += (index % columns) * clip.frame.width;
    source.y += (index / columns) * clip.frame.height;
    return source;
}

void Animations::import() {
    ecs::component<Animation>().with<Sprite, Animator>();
    (void)ecs::event<AnimationFinished>();

    // Runs just before drawing, so sprites created anywhere earlier in the frame never show
    // their whole texture for a frame.
    const auto phase = ecs::phase("ray.Animate").after(EcsPreRender).before(EcsOnRender).id();
    ecs::system("ray.Animate")
        .phase(phase)
        .exclude_up<ecs::ChildOf, SimulationPaused>()
        .immediate()
        .each([](ecs::entity entity,
                 const Animation &clip,
                 Animator &state,
                 Sprite &sprite,
                 ecs::res<const DeltaTime> delta) {
            if (!state.paused)
                state.time += delta->value;
            state.frame = frame_index(clip, state.time);
            sprite.source = frame_source(clip, state.frame);
            if (!state.paused && clip.playback == Playback::once &&
                state.frame + 1 >= clip.frames) {
                state.paused = true;
                ecs::trigger<AnimationFinished>(entity);
            }
        });
}

static bool same_clip(const Animation &a, const Animation &b) {
    return a.frame.x == b.frame.x && a.frame.y == b.frame.y && a.frame.width == b.frame.width &&
           a.frame.height == b.frame.height && a.frames == b.frames && a.fps == b.fps &&
           a.playback == b.playback && a.columns == b.columns;
}

void play(ecs::entity entity, const Animation &clip) {
    if (const Animation *current = entity.try_get<Animation>();
        current && same_clip(*current, clip))
        return;
    entity.set(clip, Animator{});
}

} // namespace ray
