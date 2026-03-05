// input.cpp                                                          -*-C++-*-
#include <core/input.h>

// core
#include <core/window.h>

// imgui
#include <imgui.h>

// std
#include <ranges>

namespace engine::core {

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

    const glm::vec2 currentMouse = platform->getMousePosition();
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

}  // close engine::ui namespace