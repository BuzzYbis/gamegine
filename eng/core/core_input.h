// core_input.h                                                       -*-C++-*-
#ifndef INCLUDED_ENG_CORE_INPUT_H
#define INCLUDED_ENG_CORE_INPUT_H

//@PURPOSE: Provide a manager for handling user input from keyboard and mouse.
//
//@CLASSES:
//  eng::core::InputAction: Enumeration of logical engine actions.
//  eng::core::InputManager: Central class for tracking and querying input
//  state.
//
//@DESCRIPTION: This component provides the 'eng::core::InputManager' class,
// which abstracts raw GLFW input into logical 'eng::core::InputAction' events.
// It tracks the state of keys and mouse buttons, calculates mouse movement
// deltas, and handles viewport-specific focus and hover states.

// std
#include <unordered_map>

// core
#include <core/core_window.h>

// third-party
#include <glm/vec2.hpp>

namespace eng::core {

/// Enumerates the high-level logical actions that can be triggered by input.
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

// ==================
// class InputManager
// ==================

/// This class is responsible for managing the input state and mapping raw
/// hardware events to logical game actions.
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
    // MANIPULATORS

    /// Bind the specified GLFW 'glfwKey' to the specified logical 'action'.
    void bindKey(int glfwKey, InputAction action);

    /// Update the internal input state based on the current state of the
    /// specified 'window' and the elapsed time 'dt'.
    void update(const Window* window, float dt);

    /// Set the viewport focus state to the specified 'focused'.
    void setViewportFocused(bool focused);

    /// Set the viewport hover state to the specified 'hovered'.
    void setViewportHovered(bool hovered);

    // ACCESSORS

    /// Return 'true' if the specified 'action' is currently being triggered
    /// (e.g., the bound key is pressed), and 'false' otherwise.
    bool isActive(InputAction action);

    /// Return the mouse movement delta since the last 'update' call.
    [[nodiscard]]
    glm::vec2 mouseDelta() const;
};

inline glm::vec2 InputManager::mouseDelta() const
{
    return d_mouseDelta;
}

inline void InputManager::setViewportFocused(const bool focused)
{
    d_viewportFocused = focused;
}

inline void InputManager::setViewportHovered(const bool hovered)
{
    d_viewportHovered = hovered;
}

}  // close package namespace
#endif  // INCLUDED_ENG_CORE_INPUT_H
