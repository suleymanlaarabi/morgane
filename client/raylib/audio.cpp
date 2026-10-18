#include "audio.h"
#include <algorithm>
#include <vector>

namespace ray {
namespace {
/** Resource: sounds made by make_sound, unloaded before CloseAudioDevice. */
struct Sounds {
    std::vector<::Sound> loaded;
    bool ready = false;
};

void apply_volume(const Volume &volume) { SetMasterVolume(std::clamp(volume.master, 0.0f, 1.0f)); }
} // namespace

void Audio::import(const Volume &volume) {
    InitAudioDevice();
    ecs::set_resource(Sounds{ .ready = IsAudioDeviceReady() });
    (void)ecs::resource_handle<Volume>({ .on_set = apply_volume });
    ecs::set_resource(volume);
    ecs::at_fini([] {
        auto &sounds = ecs::resource<Sounds>();
        for (auto &sound : sounds.loaded)
            UnloadSound(sound);
        sounds.loaded.clear();
        if (sounds.ready)
            CloseAudioDevice();
    });
}

Sound make_sound(std::span<const float> samples, int sample_rate) {
    auto &sounds = ecs::resource<Sounds>();
    if (!sounds.ready)
        return {};
    std::vector<short> pcm(samples.size());
    std::ranges::transform(samples, pcm.begin(), [](float sample) {
        return short(std::clamp(sample, -1.0f, 1.0f) * 32767);
    });
    const Wave wave{
        .frameCount = unsigned(pcm.size()),
        .sampleRate = unsigned(sample_rate),
        .sampleSize = 16,
        .channels = 1,
        .data = pcm.data(),
    };
    sounds.loaded.push_back(LoadSoundFromWave(wave));
    return { std::uint32_t(sounds.loaded.size() - 1) };
}

void play(Sound sound, float gain, float pitch) {
    auto &sounds = ecs::resource<Sounds>();
    if (sound.id >= sounds.loaded.size())
        return;
    const auto &loaded = sounds.loaded[sound.id];
    SetSoundVolume(loaded, gain);
    SetSoundPitch(loaded, pitch);
    PlaySound(loaded);
}

} // namespace ray
