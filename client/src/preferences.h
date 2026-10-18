#pragma once

#include <client/raylib/input.h>
#include <shared/game.h>
#include <string>

namespace game {

ray::Bindings<Action> default_bindings();

/** Resource: player settings, saved between runs. Edit it, then call apply_preferences(). */
struct Preferences {
    float volume = 0.8f;
    bool flashes = true;     // blink damaged ships; off keeps them steadily tinted instead
    bool large_text = false; // larger UI text
    ray::Bindings<Action> bindings = default_bindings();
};

/** Display name of a key, for the controls shown on screen. */
std::string key_name(KeyboardKey key);
/** Every key bound to `action`, separated by '/'. */
std::string action_keys(const ray::Bindings<Action> &bindings, Action action);

/** Push Preferences into the volume, input bindings and UI theme resources. */
void apply_preferences();
/** Write Preferences to the user's config directory; failures keep the game running. */
void save_preferences();

/** Module: loads Preferences from the user's config directory and applies them. Import after
 * ray::Audio, ray::Inputs<Action> and gui::Gui. */
struct PreferencesModule {
    static void import();
};

} // namespace game
