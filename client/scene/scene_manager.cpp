#include <stdexcept>
#include "scene_manager.h"
#include "siecs.h"

SceneManager &SceneManager::get()
{
    static SceneManager instance;
    return instance;
}

void SceneManager::install()
{
    ecs::system("scene.Apply").phase(EcsOnLoad).each([] {SceneManager::get().apply();});
}

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
