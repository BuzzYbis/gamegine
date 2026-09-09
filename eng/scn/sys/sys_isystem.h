// sys_isystem.h                                                      -*-C++-*-
#ifndef INCLUDED_ENG_SCN_SYS_ISYSTEM_H
#define INCLUDED_ENG_SCN_SYS_ISYSTEM_H

//@PURPOSE: Provide an interface for the systems of the scene.
//
//@CLASSES:
//  eng::scn::sys::ISystem: The interface for the systems of the scene.
//
//@DESCRIPTION: This component provides an interface for the systems of the
// scene. It is responsible for updating the systems of the scene.

// entt
#include <entt/entt.hpp>

// Forward namespace
namespace eng::core {
class InputManager;
}  // close package namespace

namespace eng::scn::sys {

// =============
// class ISystem
// =============

/// Interface for the systems of the scene.
class ISystem {
  public:
    // CREATORS

    virtual ~ISystem() = default;

    // MANIPULATORS

    /// Update the system.
    virtual void
    update(entt::registry& registry, core::InputManager& input, float dt) = 0;

    // ACCESSORS

    /// Return the name under which this system is profiled. Systems sharing
    /// a name share a single profiling scope, and hence report the sum of
    /// their durations.
    [[nodiscard]]
    virtual const char* name() const
    {
        return "Unnamed System";
    }
};

}  // close package namespace
#endif  // INCLUDED_ENG_SCN_SYS_ISYSTEM_H
