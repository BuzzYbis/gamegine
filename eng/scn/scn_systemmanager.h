// scn_systemmanager.h                                                -*-C++-*-
#ifndef INCLUDED_SCN_SYSTEM_MANAGER_H
#define INCLUDED_SCN_SYSTEM_MANAGER_H

//@PURPOSE: Provide a manager for the systems of the scene.
//
//@CLASSES:
//  eng::scn::SystemManager: Orchestrator for the systems of the scene.
//
//@DESCRIPTION: This component provides a centralized manager,
// 'eng::scn::SystemManager', responsible for owning and updating all ECS
// systems in the scene. It ensures that systems are executed in the correct
// order during the game loop.

// std
#include <memory>
#include <vector>

// scene
#include <scn/sys/sys_isystem.h>

// Forward declarations
namespace eng::core {
class InputManager;
}  // close package namespace

namespace eng::scn {

// ===================
// class SystemManager
// ===================

/// This class is responsible for managing the lifecycle and update loop of
/// ECS systems.
class SystemManager {
  private:
    // DATA

    /// List of all systems registered in the scene.
    std::vector<std::unique_ptr<sys::ISystem> > d_systems;

  public:
    // MANIPULATORS

    /// Register the specified 'system' in the scene. The manager takes
    /// ownership of the provided system.
    void registerSystem(std::unique_ptr<sys::ISystem> system);

    /// Iterate through all registered systems and invoke their 'update'
    /// methods using the specified 'registry', 'input', and delta time 'dt'.
    void updateAll(entt::registry&     registry,
                   core::InputManager& input,
                   float               dt) const;
};

}  // close package namespace
#endif  // INCLUDED_SCN_SYSTEM_MANAGER_H
