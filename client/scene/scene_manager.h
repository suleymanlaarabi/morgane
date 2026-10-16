#pragma once

#include <client.h>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace scene {
    using Enter = std::function<void(ecs::entity root)>;

    struct SceneDef {
        std::string name;
        Enter enter;
    };

    struct ScenesConfig {
        std::vector<SceneDef> scenes;
        std::string initial;
    };

    class SceneManager {
        public:
            void add(const std::string &name, Enter enter);
            void switchTo(const std::string &name);
            const std::string &current() const {
                return _current;
            }
            void apply();
        private:
            std::unordered_map<std::string, Enter> _scenes;
            std::optional<ecs::entity> _root;
            std::string _current;
            std::string _requested;
    };

    struct Scenes {
        static void import(const ScenesConfig &config);
    };

    void switchTo(const std::string &name);
}
