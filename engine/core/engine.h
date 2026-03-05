// engine.h                                                           -*-C++-*-
#ifndef INCLUDED_ENGINE_CORE_ENGINE_H
#define INCLUDED_ENGINE_CORE_ENGINE_H

#include "asset/asset_manager.h"

#include <memory>

#include <core/input.h>
#include <core/step_timer.h>
#include <core/window.h>

#include <renderer/renderer.h>

#include <scene/entity.h>
#include <scene/system_manager.h>
#include <ui/ui_manager.h>

namespace engine::core {
// ------------
// class Engine
// ------------

///
class Engine {
  private:
    // DATA
    std::unique_ptr<Window>               d_window;
    std::unique_ptr<InputManager>         d_inputManager;
    std::unique_ptr<renderer::Renderer>   d_renderer;
    std::unique_ptr<asset::AssetManager>  d_assetManager;
    std::unique_ptr<ui::UIManager>        d_uiManager;
    std::unique_ptr<scene::Scene>         d_scene;
    std::unique_ptr<scene::SystemManager> d_systemManager;
    StepTimer                             d_timer;

    // PRIVATE CLASS METHODS

    void clean();

  public:
    // CLASS METHODS

    void initialize(const char* title, int width, int height);

    void run();

    scene::Entity createEntity(const std::string& name) const;

    // CREATORS

    Engine() = default;

    ~Engine();

    // ACCESSORS

    asset::AssetManager*  assetManager() const;
    renderer::Renderer*   renderer() const;
    InputManager*         inputManager() const;
    scene::SystemManager* systemManager() const;
    scene::Scene*         scene() const;
};

inline asset::AssetManager* Engine::assetManager() const
{
    return d_assetManager.get();
}

inline renderer::Renderer* Engine::renderer() const
{
    return d_renderer.get();
}

inline InputManager* Engine::inputManager() const
{
    return d_inputManager.get();
}

inline scene::SystemManager* Engine::systemManager() const
{
    return d_systemManager.get();
}

inline scene::Scene* Engine::scene() const
{
    return d_scene.get();
}
}  // close core namespace
#endif  // INCLUDED_ENGINE_CORE_ENGINE_H
