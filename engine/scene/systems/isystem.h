// isystem.h                                                          -*-C++-*-
#ifndef INCLUDED_ENGINE_SCENE_SYSTEMS_ISYSTEM_H
#define INCLUDED_ENGINE_SCENE_SYSTEMS_ISYSTEM_H

//@PURPOSE: Provide an interface for the systems of the scene.
//
//@CLASSES:
//  engine::scene::ISystem: The interface for the systems of the scene.
//
//@DESCRIPTION: This component provides an interface for the systems of the
// scene. It is responsible for updating the systems of the scene.

// entt
#include <entt/entt.hpp>

// Forward namespace
namespace engine::core {
class InputManager;
}  // close engine::core namespace

namespace engine::scene {

// -------------
// class ISystem
// -------------

/// Interface for the systems of the scene.
class ISystem {
  public:
    // CREATORS

    virtual ~ISystem() = default;

    // MANIPULATORS

    /// Update the system.
    virtual void
    update(entt::registry& registry, core::InputManager& input, float dt) = 0;
};

}  // close engine::scene namespace

#endif  // INCLUDED_ENGINE_SCENE_SYSTEMS_ISYSTEM_H
