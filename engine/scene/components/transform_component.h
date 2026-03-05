// transform_component.h                                              -*-C++-*-
#ifndef INCLUDED_ENGINE_SCENE_COMPONENTS_TRANSFORM_COMPONENT_H
#define INCLUDED_ENGINE_SCENE_COMPONENTS_TRANSFORM_COMPONENT_H

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace engine::scene {

struct TransformComponent {
    // DATA
    glm::vec3 d_translation = {0.0f, 0.0f, 0.0f};
    glm::vec3 d_rotation    = {0.0f, 0.0f, 0.0f};
    glm::vec3 d_scale       = {1.0f, 1.0f, 1.0f};

    // CREATORS
    TransformComponent() = default;
    explicit TransformComponent(const glm::vec3& pos)
    : d_translation(pos)
    {
    }

    // MANIPULATORS

    void setPosition(const glm::vec3& position) { d_translation = position; }

    void setRotation(const glm::vec3& rotation) { d_rotation = rotation; }

    void setScale(const glm::vec3& scale) { d_scale = scale; }

    // ACCESSORS

    [[nodiscard]] glm::mat4 mat4() const
    {
        auto transform = glm::mat4(1.0f);

        transform = glm::translate(transform, d_translation);

        transform = glm::rotate(transform, d_rotation.x, {1, 0, 0});
        transform = glm::rotate(transform, d_rotation.y, {0, 1, 0});
        transform = glm::rotate(transform, d_rotation.z, {0, 0, 1});

        transform = glm::scale(transform, d_scale);

        return transform;
    }
};

}  // close engine::scene namespace

#endif  // INCLUDED_ENGINE_SCENE_COMPONENTS_TRANSFORM_COMPONENT_H