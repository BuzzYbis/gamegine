// comp_camera.h                                                      -*-C++-*-
#ifndef INCLUDED_SCN_COMP_CAMERA_H
#define INCLUDED_SCN_COMP_CAMERA_H

//@PURPOSE: Provide a component representing a camera in the scene.
//
//@CLASSES:
//  eng::scn::comp::CameraComponent: ECS component for camera data.
//
//@DESCRIPTION: This component stores the properties of a camera, such as
// field of view, aspect ratio, and near/far clipping planes. It provides
// utility methods to generate projection and view matrices for rendering.

// scene
#include <scn/comp/comp_transform.h>

// third-party
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace eng::scn::comp {

// ======================
// struct CameraComponent
// ======================

/// This component defines the viewing frustum and camera properties for an
/// entity.
struct CameraComponent {
    // DATA
    float fov         = 45.0f;     // Field of view in degrees.
    float aspectRatio = 1.77f;     // Viewport aspect ratio (width / height).
    float nearPlane   = 0.1f;      // Near clipping plane distance.
    float farPlane    = 10000.0f;  // Far clipping plane distance.

    // MANIPULATORS

    /// Set the aspect ratio of this camera to the specified 'ratio'.
    void setAspectRatio(float ratio) { aspectRatio = ratio; }

    // ACCESSORS

    /// Return the perspective projection matrix based on current camera
    /// settings.
    [[nodiscard]]
    glm::mat4 getProjection() const
    {
        return glm::perspective(glm::radians(fov),
                                aspectRatio,
                                nearPlane,
                                farPlane);
    }

    /// Return the view matrix derived from the specified 'transform'
    /// component.
    [[nodiscard]]
    static glm::mat4 getViewMatrix(const TransformComponent& transform)
    {
        const float yaw   = transform.d_rotation.y;
        const float pitch = transform.d_rotation.x;

        glm::vec3 front;
        front.x = std::cos(yaw) * std::cos(pitch);
        front.y = std::sin(pitch);
        front.z = std::sin(yaw) * std::cos(pitch);

        const glm::vec3 cameraFront = glm::normalize(front);
        const glm::vec3 eye         = transform.d_translation;

        return glm::lookAt(eye,
                           eye + cameraFront,
                           glm::vec3(0.0f, 1.0f, 0.0f));
    }
};

}  // close package namespace
#endif  // INCLUDED_SCN_COMP_CAMERA_H
