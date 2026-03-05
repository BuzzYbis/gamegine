// camera_system.cpp                                                -*-C++-*-
#include <scene/systems/camera_system.h>

// core
#include <core/input.h>

// scene
#include <scene/components/camera_component.h>
#include <scene/components/transform_component.h>
#include <scene/scene.h>

namespace engine::scene {

void CameraSystem::update(entt::registry&     registry,
                          core::InputManager& inputManager,
                          const float         dt)
{
    const auto view = registry.view<CameraComponent, TransformComponent>();
    for (const auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);

        const float yaw   = transform.d_rotation.y;
        const float pitch = transform.d_rotation.x;

        glm::vec3 front;
        front.x = cos(yaw) * cos(pitch);
        front.y = sin(pitch);
        front.z = sin(yaw) * cos(pitch);
        front   = glm::normalize(front);

        glm::vec3 right = glm::normalize(
            glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

        glm::vec2 mouseDelta = inputManager.getMouseDelta();
        if ((mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) &&
            inputManager.isActive(core::InputAction::RIGHT_CLICK)) {
            constexpr float sensitivity = 0.003f;
            transform.d_rotation.y += mouseDelta.x * sensitivity;
            transform.d_rotation.x -= mouseDelta.y * sensitivity;

            if (transform.d_rotation.x > 1.5f)
                transform.d_rotation.x = 1.5f;
            if (transform.d_rotation.x < -1.5f)
                transform.d_rotation.x = -1.5f;
        }

        const float velocity = 6.0f * dt;

        if (inputManager.isActive(core::InputAction::MOVE_FORWARD)) {
            transform.d_translation += front * velocity;
        }

        if (inputManager.isActive(core::InputAction::MOVE_BACKWARD)) {
            transform.d_translation -= front * velocity;
        }

        if (inputManager.isActive(core::InputAction::MOVE_LEFT)) {
            transform.d_translation -= right * velocity;
        }

        if (inputManager.isActive(core::InputAction::MOVE_RIGHT)) {
            transform.d_translation += right * velocity;
        }

        if (inputManager.isActive(core::InputAction::MOVE_UP)) {
            transform.d_translation.y += velocity;
        }

        if (inputManager.isActive(core::InputAction::MOVE_DOWN)) {
            transform.d_translation.y -= velocity;
        }
    }
}

}  // close engine::scene namespace
