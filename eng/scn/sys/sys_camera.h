// sys_camera.h                                                       -*-C++-*-
#ifndef INCLUDED_SCN_SYS_CAMERA_H
#define INCLUDED_SCN_SYS_CAMERA_H

//@PURPOSE: Provide a system for managing and updating camera components.
//
//@CLASSES:
//  eng::scn::sys::CameraSystem: ECS system for camera control.
//
//@DESCRIPTION: This component provides the 'eng::scn::system::CameraSystem'
// class, which is responsible for processing entities with camera and
// transform components. it handles user input to update the camera's
// orientation and position in the scene.

// scene
#include <scn/sys/sys_isystem.h>

namespace eng::scn::sys {

// ==================
// class CameraSystem
// ==================

/// This system handles the update logic for cameras, including input
/// processing.
class CameraSystem : public ISystem {
  public:
    // MANIPULATORS

    /// Update all camera entities in the specified 'registry' using the
    /// specified 'inputManager' and delta time 'dt'.
    void update(entt::registry&     registry,
                core::InputManager& inputManager,
                float               dt) override;

    // ACCESSORS

    /// Return the name under which this system is profiled.
    [[nodiscard]]
    const char* name() const override
    {
        return "Camera System";
    }
};

}  // close package namespace
#endif  // INCLUDED_SCN_SYS_CAMERA_H