// core_engine.h                                                      -*-C++-*-
#ifndef INCLUDED_ENG_CORE_ENGINE_H
#define INCLUDED_ENG_CORE_ENGINE_H

//@PURPOSE: Provide the main entry point and orchestrator for the game engine.
//
//@CLASSES:
//  eng::core::Engine: Central class managing the engine's lifecycle and
//  subsystems.
//
//@DESCRIPTION: This component provides the 'eng::core::Engine' class, which
// serves as the top-level container for all engine subsystems (Renderer,
// Scene, Input, Assets, UI). It is responsible for initializing the RHI,
// managing the main game loop, and coordinating updates across all systems.

// std
#include <memory>

// asset
#include <asset/asset_manager.h>

// core
#include <core/core_input.h>
#include <core/core_steptimer.h>
#include <core/core_window.h>

// rhi
#include <rhi/rhi_contextprotocol.h>

// rnd
#include <rnd/renderer.h>

// scn
#include <scn/scn_entity.h>
#include <scn/scn_systemmanager.h>

// ui
#include <ui/ui_manager.h>

namespace eng::core {

// ============
// class Engine
// ============

/// This class is responsible for the overall management of the game engine,
/// including initialization, the main loop, and subsystem coordination.
class Engine {
  private:
    // DATA

    std::unique_ptr<Window>               d_window;
    std::unique_ptr<rhi::ContextProtocol> d_rhiContext;
    std::unique_ptr<InputManager>         d_inputManager;
    std::unique_ptr<rnd::Renderer>        d_renderer;
    std::unique_ptr<asset::AssetManager>  d_assetManager;
    std::unique_ptr<ui::UIManager>        d_uiManager;
    std::unique_ptr<scn::Scene>           d_scene;
    std::unique_ptr<scn::SystemManager>   d_systemManager;
    StepTimer                             d_timer;
    u_int32_t                             d_lastFps;

    // PRIVATE MANIPULATORS

    /// Release all engine resources and shut down subsystems.
    void clean();

  public:
    // CREATORS

    /// Create an 'Engine' instance. Subsystems are not initialized until
    /// 'initialize' is called.
    Engine() = default;

    /// Destroy this engine and its subsystems.
    ~Engine();

    // MANIPULATORS

    /// Initialize the engine window with the specified 'title', 'width',
    /// and 'height', and bootstrap all core subsystems.
    void initialize(const char* title, int width, int height);

    /// Enter the main game loop. This method does not return until the
    /// engine is shut down.
    void run();

    // ACCESSORS

    /// Create and return a new entity with the specified 'name' in the
    /// current scene.
    [[nodiscard]]
    scn::Entity createEntity(const std::string& name) const;

    /// Return a pointer to the asset manager subsystem.
    [[nodiscard]]
    asset::AssetManager* assetManager() const;

    /// Return a pointer to the renderer subsystem.
    [[nodiscard]]
    rnd::Renderer* renderer() const;

    /// Return a pointer to the input manager subsystem.
    [[nodiscard]]
    InputManager* inputManager() const;

    /// Return a pointer to the system manager (ECS logic).
    [[nodiscard]]
    scn::SystemManager* systemManager() const;

    /// Return a pointer to the current scene.
    [[nodiscard]]
    scn::Scene* scene() const;
};

inline asset::AssetManager* Engine::assetManager() const
{
    return d_assetManager.get();
}

inline rnd::Renderer* Engine::renderer() const
{
    return d_renderer.get();
}

inline InputManager* Engine::inputManager() const
{
    return d_inputManager.get();
}

inline scn::SystemManager* Engine::systemManager() const
{
    return d_systemManager.get();
}

inline scn::Scene* Engine::scene() const
{
    return d_scene.get();
}

}  // close core namespace
#endif  // INCLUDED_ENG_CORE_ENGINE_H
