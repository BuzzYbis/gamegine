// core_input.cpp                                                     -*-C++-*-
#include <core/core_input.h>

// std
#include <ranges>

// core
#include <core/core_window.h>

// third-party
#include <imgui.h>

namespace eng::core {

void InputManager::bindKey(const int glfwKey, const InputAction action)
{
    d_keyBindings[glfwKey] = action;
}

bool InputManager::isActive(const InputAction action)
{
    const auto it = d_actionStates.find(action);
    if (it != d_actionStates.end()) {
        return it->second;
    }
    return false;
}

void InputManager::update(const Window* platform, float dt)
{
    for (auto& state : d_actionStates | std::views::values) {
        state = false;
    }

    const ImGuiIO& io = ImGui::GetIO();

    if (!(io.WantCaptureKeyboard && !d_viewportFocused)) {
        for (auto const& [key, action] : d_keyBindings) {
            if (platform->isKeyPressed(key)) {
                d_actionStates[action] = true;
            }
        }
    }

    const glm::vec2 currentMouse = platform->mousePosition();
    const bool      blockMouse   = io.WantCaptureMouse && !d_viewportHovered;

    if (!blockMouse) {
        d_mouseDelta = currentMouse - d_lastMousePos;

        for (auto const& [key, action] : d_keyBindings) {
            if (platform->isMouseButtonPressed(key)) {
                d_actionStates[action] = true;
            }
        }
    }
    else {
        d_mouseDelta = glm::vec2(0.0f);
    }
    d_lastMousePos = currentMouse;
}

}  // close package namespace