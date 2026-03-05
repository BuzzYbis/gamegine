// camera_component.h                                                 -*-C++-*-
#ifndef INCLUDED_ENGINE_SCENE_COMPONENTS_CAMERA_COMPONENT_H
#define INCLUDED_ENGINE_SCENE_COMPONENTS_CAMERA_COMPONENT_H

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>

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
};
}  // close engine::scene namespace
#endif  // INCLUDED_ENGINE_SCENE_COMPONENTS_CAMERA_COMPONENT_H
