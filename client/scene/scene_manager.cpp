#include <stdexcept>
#include "scene_manager.h"
#include "siecs.h"


namespace scene {

    void SceneManager::add(SceneId id, Enter enter)
    {
        _scenes[id] = std::move(enter);
    }

    void SceneManager::switchTo(SceneId id)
    {
        _requested = id;
    }

    void SceneManager::apply()
    {
        if (!_requested)
            return;

        auto it = _scenes.find(*_requested);
        if (it == _scenes.end())
            throw std::runtime_error(std::string("Unknown scene: ") + _requested->name());

        if (_root)
            _root->kill();

        _root = ecs::entity::create();
        _current = *_requested;
        _requested.reset();
        it->second(*_root);
    }

    void Scenes::import(const ScenesConfig &config)
    {
        SceneManager manager;
        for (const auto &def : config.scenes)
            manager.add(def.id, def.enter);
        if (config.initial)
            manager.switchTo(*config.initial);
        ecs::set_resource(manager);

        ecs::system("scene.Apply").phase(EcsOnLoad).each([](ecs::res<SceneManager> sm) {
            sm->apply();
        });
    }

    void switchTo(SceneId id)
    {
        ecs::resource<SceneManager>().switchTo(id);
    }
}
