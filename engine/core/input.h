// input.h                                                            -*-C++-*-
#ifndef INCLUDED_ENGINE_CORE_INPUT_H
#define INCLUDED_ENGINE_CORE_INPUT_H

#include <core/window.h>

#include <glm/vec2.hpp>

#include <unordered_map>

namespace engine::core {
enum class InputAction {
    MOVE_FORWARD,
    MOVE_BACKWARD,
    MOVE_LEFT,
    MOVE_RIGHT,
    MOVE_UP,
    MOVE_DOWN,
    LOOK_UP,
    LOOK_DOWN,
    LOOK_LEFT,
    LOOK_RIGHT,
    RIGHT_CLICK,
};

class InputManager {
  private:
    // DATA

    std::unordered_map<int, InputAction>  d_keyBindings  = {};
    std::unordered_map<InputAction, bool> d_actionStates = {};

    glm::vec2 d_lastMousePos{0.0f};
    glm::vec2 d_mouseDelta{0.0f};

    bool d_viewportFocused = false;
    bool d_viewportHovered = false;

  public:
    // CLASS METHODS

    void bindKey(int glfwKey, InputAction action);

    bool isActive(InputAction action);

    [[nodiscard]] glm::vec2 getMouseDelta() const { return d_mouseDelta; }

    void update(const Window* platform, float dt);
    // CREATORS

    InputManager() = default;

    // MANIPULATORS

    void setViewportFocused(bool focused);
    void setViewportHovered(bool hovered);
};

inline void InputManager::setViewportFocused(const bool focused)
{
    d_viewportFocused = focused;
}

inline void InputManager::setViewportHovered(const bool hovered)
{
    d_viewportHovered = hovered;
}

}  // close engine::core namespace

#endif  // INCLUDED_ENGINE_CORE_INPUT_H
