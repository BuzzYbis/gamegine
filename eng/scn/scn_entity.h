// entity.h                                                           -*-C++-*-
#ifndef INCLUDED_SCN_ENTITY_H
#define INCLUDED_SCN_ENTITY_H

//@PURPOSE: Provide an entity for the game.
//
//@CLASSES:
//  eng::scn::Entity: The entity of the game.
//
//@DESCRIPTION: This component provides an entity for the game. It is
// responsible
// for creating, destroying components and handling the entity of the game.

// std
#include <cassert>

// scene
#include <scn/scn_scene.h>

namespace eng::scn {

// ============
// class Entity
// ============

/// This class provides an object-oriented wrapper around a raw EnTT entity
/// handle. It provides a convenient API for component management and
/// ensures type safety via assertions.
class Entity {
  private:
    // DATA

    /// The raw identifier used by the EnTT library.
    entt::entity d_entityHandle{entt::null};

    /// Pointer to the scene that owns this entity.
    Scene* d_scene = nullptr;

  public:
    // CREATORS

    /// Create a null entity wrapper.
    Entity() = default;

    /// Create an entity wrapper for the specified 'handle' in the specified
    /// 'scene'.
    Entity(const entt::entity handle, Scene* scene)
    : d_entityHandle(handle)
    , d_scene(scene)
    {
    }

    // MANIPULATORS

    /// Add a component of type 'T' to this entity, forwarding the specified
    /// 'args' to its constructor. Return a reference to the newly created
    /// component. The behavior is undefined if this entity already has a
    /// component of type 'T'.
    template <typename T, typename... Args>
    T& addComponent(Args&&... args)
    {
        assert(!hasComponent<T>() && "Entity already has component!");
        return d_scene->d_registry.emplace<T>(d_entityHandle,
                                              std::forward<Args>(args)...);
    }

    /// Return a reference to the component of type 'T' attached to this
    /// entity. The behavior is undefined if no such component exists.
    template <typename T>
    T& getComponent()
    {
        assert(hasComponent<T>() && "Entity does not have component!");
        return d_scene->d_registry.get<T>(d_entityHandle);
    }

    /// Remove the component of type 'T' from this entity. The behavior is
    /// undefined if no such component exists.
    template <typename T>
    void removeComponent()
    {
        assert(hasComponent<T>() && "Entity does not have component!");
        d_scene->d_registry.remove<T>(d_entityHandle);
    }

    // ACCESSORS

    /// Return 'true' if this entity has a component of type 'T', and
    /// 'false' otherwise.
    template <typename T>
    [[nodiscard]]
    bool hasComponent() const
    {
        return d_scene->d_registry.all_of<T>(d_entityHandle);
    }

    /// Return 'true' if this entity handle is not null, and 'false'
    /// otherwise.
    explicit operator bool() const { return d_entityHandle != entt::null; }

    /// Return 'true' if this entity is identical to the specified 'other'
    /// entity, and 'false' otherwise.
    bool operator==(const Entity& other) const
    {
        return d_entityHandle == other.d_entityHandle &&
               d_scene == other.d_scene;
    }

    /// Return 'true' if this entity is not identical to the specified
    /// 'other' entity, and 'false' otherwise.
    bool operator!=(const Entity& other) const { return !(*this == other); }

    /// Return the raw integer ID of this entity.
    [[nodiscard]]
    uint32_t id() const;

    /// Return the raw EnTT handle of this entity.
    [[nodiscard]]
    entt::entity entityHandle() const;
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

}  // close package namespace
#endif  //  INCLUDED_SCN_ENTITY_H