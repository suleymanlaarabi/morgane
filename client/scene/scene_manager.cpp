#include <stdexcept>
#include "scene_manager.h"
#include "siecs.h"


namespace scene {

    void SceneManager::add(const std::string &name, Enter enter)
    {
        _scenes[name] = std::move(enter);
    }

    void SceneManager::switchTo(const std::string &name)
    {
        _requested = name;
    }

    void SceneManager::apply()
    {
        if (_requested.empty())
            return;

        auto it = _scenes.find(_requested);
        if (it == _scenes.end())
            throw std::runtime_error("Unknown scene: " + _requested);

        if (_root)
            _root->kill();

        _root = ecs::entity::create();
        _current = _requested;
        _requested.clear();
        it->second(*_root);
    }

    void Scenes::import(const ScenesConfig &config)
    {
        SceneManager manager;
        for (const auto &def : config.scenes)
            manager.add(def.name, def.enter);
        if (!config.initial.empty())
            manager.switchTo(config.initial);
        ecs::set_resource(manager);

        ecs::system("scene.Apply").phase(EcsOnLoad).each([](ecs::res<SceneManager> sm) {
            sm->apply();
        });
    }

    void switchTo(const std::string &name)
    {
        ecs::resource<SceneManager>().switchTo(name);
    }
}
