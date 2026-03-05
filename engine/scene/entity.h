// entity.h                                                           -*-C++-*-
#ifndef INCLUDED_ENGINE_SCENE_ENTITY_H
#define INCLUDED_ENGINE_SCENE_ENTITY_H

//@PURPOSE: Provide an entity for the game.
//
//@CLASSES:
//  engine::scene::Entity: The entity of the game.
//
//@DESCRIPTION: This component provides an entity for the game. It is responsible
// for creating, destroying components and handling the entity of the game.

// scene
#include <scene/scene.h>

// std
#include <cassert>

namespace engine::scene {

// ------------
// class Entity
// ------------

/// An entity is a handle (ID) and a context (Scene).
class Entity {
  private:
    // DATA

    /// Handle of the entity.
    entt::entity d_entityHandle{entt::null};

    /// Pointer to the scene.
    Scene*       d_scene = nullptr;

  public:
    // CREATORS

    Entity() = default;

    /// Create an entity with a handle and a scene.
    Entity(entt::entity handle, Scene* scene)
    : d_entityHandle(handle)
    , d_scene(scene)
    {
    }

    // MANIPULATORS

    /// Add a component to the entity. (Template variadic)
    template <typename T, typename... Args>
    T& addComponent(Args&&... args)
    {
        assert(!hasComponent<T>() && "Entity already has component!");
        return d_scene->d_registry.emplace<T>(d_entityHandle,
                                              std::forward<Args>(args)...);
    }

    /// Get a component from the entity.
    template <typename T>
    T& getComponent()
    {
        assert(hasComponent<T>() && "Entity does not have component!");
        return d_scene->d_registry.get<T>(d_entityHandle);
    }

    /// Remove a component from the entity.
    template <typename T>
    void removeComponent()
    {
        assert(hasComponent<T>() && "Entity does not have component!");
        d_scene->d_registry.remove<T>(d_entityHandle);
    }

    // ACCESSORS

    template <typename T>
    bool hasComponent() const
    {
        return d_scene->d_registry.all_of<T>(d_entityHandle);
    }

    /// Check if the entity is valid.
    operator bool() const { return d_entityHandle != entt::null; }

    bool operator==(const Entity& other) const
    {
        return d_entityHandle == other.d_entityHandle &&
               d_scene == other.d_scene;
    }

    bool operator!=(const Entity& other) const { return !(*this == other); }

    /// Convert the entity to a raw ID (for display or debug).
    [[nodiscard]] uint32_t id() const;

    /// Get the handle of the entity.
    [[nodiscard]] entt::entity entityHandle() const;
};

// =======================================================================
//                          INLINE DEFINITIONS
// =======================================================================

inline uint32_t Entity::id() const
{
    return static_cast<uint32_t>(d_entityHandle);
}

inline entt::entity Entity::entityHandle() const
{
    return d_entityHandle;
}

}  // close engine::scene namespace

#endif  //  INCLUDED_ENGINE_SCENE_ENTITY_H