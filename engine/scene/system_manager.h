// system_manager.h                                                   -*-C++-*-
#ifndef INCLUDED_ENGINE_SCENE_SYSTEM_MANAGER_H
#define INCLUDED_ENGINE_SCENE_SYSTEM_MANAGER_H

//@PURPOSE: Provide a manager for the systems of the scene.
//
//@CLASSES:
//  engine::scene::SystemManager: Orchestrator for the systems of the scene.
//
//@DESCRIPTION: This component provides a centralized manager,
// `engine::scene::SystemManager`, responsible for initializing the systems of
// the scene. It also provides a way to update all the systems of the scene.
// This is the third piece of the ECS system.

// scene
#include <scene/systems/isystem.h>

// std
#include <memory>
#include <vector>

namespace engine::scene {

// -------------------
// class SystemManager
// -------------------

/// This class is responsible for managing the systems of the scene.
class SystemManager {
  private:
    // DATA

    /// List of all systems registered in the scene
    std::vector<std::unique_ptr<ISystem> > d_systems;

  public:
    // MANIPULATORS

    /// Register a new system in the scene
    void registerSystem(std::unique_ptr<ISystem> system);

    // ACCESSORS

    /// Update all the systems of the scene
    void updateAll(entt::registry&     registry,
                   core::InputManager& input,
                   float               dt) const;
};

}  // close engine::scene namespace

#endif  // INCLUDED_ENGINE_SCENE_SYSTEM_MANAGER_H
