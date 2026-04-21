// sys_camera.cpp                                                     -*-C++-*-
#include <scn/sys/sys_camera.h>

// core
#include <core/core_input.h>

// scene
#include <scn/comp/comp_camera.h>
#include <scn/comp/comp_transform.h>
#include <scn/scn_scene.h>

namespace eng::scn::sys {

void CameraSystem::update(entt::registry&     registry,
                          core::InputManager& inputManager,
                          const float         dt)
{
    const auto view =
        registry.view<comp::CameraComponent, comp::TransformComponent>();
    for (const auto entity : view) {
        auto& transform = view.get<comp::TransformComponent>(entity);

        const float yaw   = transform.d_rotation.y;
        const float pitch = transform.d_rotation.x;

        glm::vec3 front;
        front.x = cos(yaw) * cos(pitch);
        front.y = sin(pitch);
        front.z = sin(yaw) * cos(pitch);
        front   = glm::normalize(front);

        glm::vec3 right = glm::normalize(
            glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

        const glm::vec2 mouseDelta = inputManager.mouseDelta();
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

}  // close package namespace
