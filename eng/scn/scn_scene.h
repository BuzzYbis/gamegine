// scene.h                                                            -*-C++-*-
#ifndef INCLUDED_SCN_SCENE_H
#define INCLUDED_SCN_SCENE_H

//@PURPOSE: Provide a scene for the game.
//
//@CLASSES:
//  eng::scn::Scene: The scene of the game.
//
//@DESCRIPTION: This component provides a scene for the game. It is responsible
// for creating, destroying entities and handling the registry of the scene.

// core
#include <core/core_steptimer.h>

// third-party
#include <entt/entt.hpp>

// Forward declaration
namespace eng::core {
class InputManager;
}  // close package namespace

namespace eng::scn {
class Entity;

// ===========
// class Scene
// ===========

/// This class provides a centralized manager for all entities and components
/// in the game world. It owns an 'entt::registry' and provides a high-level
/// API for lifecycle management.
class Scene {
  private:
    // DATA

    /// The underlying EnTT registry storing all entity data.
    entt::registry d_registry;

    /// Friend class to allow the Entity wrapper to access the registry.
    friend class Entity;

  public:
    // CREATORS

    /// Create an empty scene.
    Scene()  = default;

    /// Destroy this scene and all contained entities.
    ~Scene() = default;

    // MANIPULATORS

    /// Create and return a new entity with the optionally specified 'name'.
    Entity createEntity(const std::string& name = std::string());

    /// Destroy the specified 'entity' and all its associated components.
    void destroyEntity(Entity entity);

    /// Return a reference to the underlying EnTT registry.
    entt::registry& registry();
};

// =======================================================================
//                          INLINE DEFINITIONS
// =======================================================================

inline entt::registry& Scene::registry()
{
    return d_registry;
}

}  // close package namespace

#endif  // INCLUDED_SCN_SCENE_H