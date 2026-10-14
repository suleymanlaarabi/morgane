#pragma once

#include <client.h>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

class SceneManager {
    public:
        using Enter = std::function<void(ecs::entity root)>;

        static SceneManager &get();

        void install(); // call after ecs::init();

        void add(const std::string &name, Enter enter);
        void switchTo(const std::string &name);
        const std::string &current() const {
            return _current;
        }
    private:
        void apply();

        std::unordered_map<std::string, Enter> _scenes;
        std::optional<ecs::entity> _root;
        std::string _current;
        std::string _requested;
};
