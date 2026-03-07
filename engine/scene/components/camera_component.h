// camera_component.h                                                 -*-C++-*-
#ifndef INCLUDED_ENGINE_SCENE_COMPONENTS_CAMERA_COMPONENT_H
#define INCLUDED_ENGINE_SCENE_COMPONENTS_CAMERA_COMPONENT_H

// glm
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// scene
#include <scene/components/transform_component.h>

namespace engine::scene {
struct CameraComponent {
    float fov         = 45.0f;
    float aspectRatio = 1.77f;
    float nearPlane   = 0.1f;
    float farPlane    = 10000.0f;

    void setAspectRatio(const float ratio) { aspectRatio = ratio; }

    [[nodiscard]] glm::mat4 getProjection() const
    {
        // Calculation of the perspective matrix
        auto proj = glm::perspective(glm::radians(fov),
                                     aspectRatio,
                                     nearPlane,
                                     farPlane);

        // The Y axis in Vulkan is reversed compared to OpenGL.
        // If we don't do this, the image will be upside down.
        proj[1][1] *= -1;

        return proj;
    }

    [[nodiscard]] glm::mat4
    getViewMatrix(const TransformComponent& transform) const
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
}  // close engine::scene namespace
#endif  // INCLUDED_ENGINE_SCENE_COMPONENTS_CAMERA_COMPONENT_H
