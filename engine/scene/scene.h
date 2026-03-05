// scene.h                                                            -*-C++-*-
#ifndef INCLUDED_ENGINE_SCENE_SCENE_H
#define INCLUDED_ENGINE_SCENE_SCENE_H

//@PURPOSE: Provide a scene for the game.
//
//@CLASSES:
//  engine::scene::Scene: The scene of the game.
//
//@DESCRIPTION: This component provides a scene for the game. It is responsible
// for creating, destroying entities and handling the registry of the scene.

// core
#include <core/step_timer.h>

// entt
#include <entt/entt.hpp>

// Forward declaration
namespace engine::core {
class InputManager;
}  // close engine::core namespace

namespace engine::scene {
class Entity;

// -----------
// class Scene
// -----------

/// The scene of the game.
class Scene {
  private:
    // DATA

    /// This data will contain all the entities and their components
    entt::registry d_registry;

    /// Friend class to allow the Entity class to access the registry.
    friend class Entity;

  public:
    // CREATORS

    Scene()  = default;
    ~Scene() = default;

    // MANIPULATORS

    /// Create an empty entity.
    Entity createEntity(const std::string& name = std::string());

    /// Destroy an entity.
    void destroyEntity(Entity entity);

    /// Get the registry of the scene.
    entt::registry& registry();
};

// =======================================================================
//                          INLINE DEFINITIONS
// =======================================================================

inline entt::registry& Scene::registry()
{
    return d_registry;
}

}  // close engine::scene namespace

#endif  // INCLUDED_ENGINE_SCENE_SCENE_H