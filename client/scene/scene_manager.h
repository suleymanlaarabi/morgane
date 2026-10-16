#pragma once

#include <client.h>
#include <functional>
#include <optional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace scene {
    using Enter = std::function<void(ecs::entity root)>;
    using SceneId = std::type_index;

    template<typename T>
    SceneId id()
    {
        return std::type_index(typeid(T));
    }

    struct SceneDef {
        SceneId id;
        Enter enter;
    };

    template<typename T>
    SceneDef def(Enter enter)
    {
        return {id<T>(), std::move(enter)};
    }

    struct ScenesConfig {
        std::vector<SceneDef> scenes;
        std::optional<SceneId> initial;
    };

    class SceneManager {
        public:
            void add(SceneId id, Enter enter);
            void switchTo(SceneId id);
            void apply();
            template<typename T>
            void add(Enter enter) {
                add(id<T>(), std::move(enter));
            }
            template<typename T>
            void switchTo() {
                switchTo(id<T>());
            }
            template<typename T>
            bool is() const {
                return _current && *_current == id<T>();
            }
            const std::optional<SceneId> &current() const {
                return _current;
            }
        private:
            std::unordered_map<SceneId, Enter> _scenes;
            std::optional<ecs::entity> _root;
            std::optional<SceneId> _current;
            std::optional<SceneId> _requested;
    };

    struct Scenes {
        static void import(const ScenesConfig &config);
    };

    void switchTo(SceneId id);

    template<typename T>
    void switchTo() {
        switchTo(id<T>());
    }
}
