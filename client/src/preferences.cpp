#include "preferences.h"
#include <client/gui/gui.h>
#include <client/raylib/audio.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace game {
namespace {
constexpr const char *action_names[] = { "up", "down", "left", "right", "fire" };
static_assert(std::size(action_names) == std::size_t(Action::count));

std::filesystem::path preferences_path() {
    if (const char *config = std::getenv("XDG_CONFIG_HOME"); config && *config)
        return std::filesystem::path(config) / "morgane" / "preferences.txt";
    if (const char *home = std::getenv("HOME"); home && *home)
        return std::filesystem::path(home) / ".config" / "morgane" / "preferences.txt";
    return {};
}

/** Unknown or malformed lines keep their defaults, so an old or edited file still loads. */
Preferences load() {
    Preferences preferences;
    const auto path = preferences_path();
    if (path.empty())
        return preferences;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream words(line);
        std::string key;
        words >> key;
        if (key == "volume") {
            float volume;
            if (words >> volume && volume >= 0 && volume <= 1)
                preferences.volume = volume;
        } else if (key == "flashes") {
            words >> preferences.flashes;
        } else if (key == "large_text") {
            words >> preferences.large_text;
        } else if (key == "bind") {
            std::string name;
            words >> name;
            for (std::size_t action = 0; action < std::size(action_names); ++action) {
                if (name != action_names[action])
                    continue;
                auto &slot = preferences.bindings.keys[action];
                std::array<KeyboardKey, ray::Bindings<Action>::max_keys> keys{};
                std::size_t count = 0;
                for (int key; count < keys.size() && words >> key && key > 0 && key < 512;)
                    keys[count++] = KeyboardKey(key);
                if (count)
                    slot = keys;
            }
        }
    }
    return preferences;
}

} // namespace

void save_preferences() {
    const auto &preferences = ecs::resource<Preferences>();
    const auto path = preferences_path();
    std::error_code error;
    if (path.empty() || (std::filesystem::create_directories(path.parent_path(), error), error))
        return;
    std::ofstream file(path);
    file << "volume " << preferences.volume << "\nflashes " << preferences.flashes
         << "\nlarge_text " << preferences.large_text << '\n';
    for (std::size_t action = 0; action < std::size(action_names); ++action) {
        file << "bind " << action_names[action];
        for (auto key : preferences.bindings.keys[action])
            if (key != KEY_NULL)
                file << ' ' << int(key);
        file << '\n';
    }
}

ray::Bindings<Action> default_bindings() {
    return ray::Bindings<Action>{}
        .bind(Action::up, { KEY_UP, KEY_W })
        .bind(Action::down, { KEY_DOWN, KEY_S })
        .bind(Action::left, { KEY_LEFT, KEY_A })
        .bind(Action::right, { KEY_RIGHT, KEY_D })
        .bind(Action::fire, { KEY_SPACE });
}

std::string key_name(KeyboardKey key) {
    if (key >= KEY_A && key <= KEY_Z)
        return std::string(1, char(key));
    if (key >= KEY_ZERO && key <= KEY_NINE)
        return std::string(1, char(key));
    if (key >= KEY_F1 && key <= KEY_F12)
        return "F" + std::to_string(key - KEY_F1 + 1);
    if (key >= KEY_KP_0 && key <= KEY_KP_9)
        return "Numpad " + std::to_string(key - KEY_KP_0);
    switch (key) {
    case KEY_UP:
        return "Up";
    case KEY_DOWN:
        return "Down";
    case KEY_LEFT:
        return "Left";
    case KEY_RIGHT:
        return "Right";
    case KEY_SPACE:
        return "Space";
    case KEY_ENTER:
        return "Enter";
    case KEY_TAB:
        return "Tab";
    case KEY_BACKSPACE:
        return "Backspace";
    case KEY_LEFT_SHIFT:
        return "Left Shift";
    case KEY_RIGHT_SHIFT:
        return "Right Shift";
    case KEY_LEFT_CONTROL:
        return "Left Ctrl";
    case KEY_RIGHT_CONTROL:
        return "Right Ctrl";
    case KEY_LEFT_ALT:
        return "Left Alt";
    case KEY_RIGHT_ALT:
        return "Right Alt";
    case KEY_INSERT:
        return "Insert";
    case KEY_DELETE:
        return "Delete";
    case KEY_HOME:
        return "Home";
    case KEY_END:
        return "End";
    case KEY_PAGE_UP:
        return "Page Up";
    case KEY_PAGE_DOWN:
        return "Page Down";
    case KEY_KP_ENTER:
        return "Numpad Enter";
    default:
        return "Key " + std::to_string(int(key));
    }
}

std::string action_keys(const ray::Bindings<Action> &bindings, Action action) {
    std::string text;
    for (auto key : bindings.keys[std::size_t(action)]) {
        if (key == KEY_NULL)
            continue;
        if (!text.empty())
            text += " / ";
        text += key_name(key);
    }
    return text.empty() ? "none" : text;
}

void apply_preferences() {
    const auto &preferences = ecs::resource<Preferences>();
    ecs::set_resource(ray::Volume{ preferences.volume });
    ecs::set_resource(preferences.bindings);
    ecs::set_resource(gui::Theme{ .font_size = preferences.large_text ? 28 : 20, .style = "dark" });
}

void PreferencesModule::import() {
    ecs::set_resource(load());
    apply_preferences();
}

} // namespace game
