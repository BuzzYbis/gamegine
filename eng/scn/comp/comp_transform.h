// comp_transform.h                                                   -*-C++-*-
#ifndef INCLUDED_SCN_COMP_TRANSFORM_H
#define INCLUDED_SCN_COMP_TRANSFORM_H

//@PURPOSE: Provide a component representing an entity's position and
// orientation.
//
//@CLASSES:
//  eng::scn::comp::TransformComponent: ECS component for 3D transformations.
//
//@DESCRIPTION: This component defines the physical presence of an entity in
// world space. It stores translation, rotation (Euler angles), and scale
// vectors, and provides a method to calculate the 4x4 model matrix.

// third-party
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace eng::scn::comp {

// =========================
// struct TransformComponent
// =========================

/// This component defines the position, rotation, and scale of an entity.
struct TransformComponent {
    // DATA
    glm::vec3 d_translation = {0.0f, 0.0f, 0.0f};  // Position in world space.
    glm::vec3 d_rotation    = {0.0f,
                               0.0f,
                               0.0f};  // Euler angles (pitch, yaw, roll).
    glm::vec3 d_scale       = {1.0f, 1.0f, 1.0f};  // Scaling factors.

    // CREATORS

    /// Create a transform with default values.
    TransformComponent() = default;

    /// Create a transform at the specified 'pos'.
    explicit TransformComponent(const glm::vec3& pos)
    : d_translation(pos)
    {
    }

    // MANIPULATORS

    /// Set the position of this transform to the specified 'position'.
    void setPosition(const glm::vec3& position) { d_translation = position; }

    /// Set the rotation of this transform to the specified 'rotation'.
    void setRotation(const glm::vec3& rotation) { d_rotation = rotation; }

    /// Set the scale of this transform to the specified 'scale'.
    void setScale(const glm::vec3& scale) { d_scale = scale; }

    // ACCESSORS

    /// Return the 4x4 model transformation matrix derived from translation,
    /// rotation, and scale data.
    [[nodiscard]]
    glm::mat4 mat4() const
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

}  // close package namespace
#endif  // INCLUDED_SCN_COMP_TRANSFORM_H