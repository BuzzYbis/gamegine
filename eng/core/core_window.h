// core_window.h                                                      -*-C++-*-
#ifndef INCLUDED_ENG_CORE_WINDOW_H
#define INCLUDED_ENG_CORE_WINDOW_H

//@PURPOSE: Provide an abstraction for system windowing and platform events.
//
//@CLASSES:
//  eng::core::Window: Abstraction of a native application window.
//
//@DESCRIPTION: This component provides the 'eng::core::Window' class, which
// wraps a GLFW window and handles the underlying OS-specific interactions
// (creation, event polling, resizing). it also provides a bridge to the
// Vulkan RHI by ensuring the necessary surface extensions are loaded.

// std
#include <string>

// third-party
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/vec2.hpp>

namespace eng::core {

// ============
// class Window
// ============

/// This class manages the lifecycle and event processing for the application's
/// graphical window.
class Window {
  private:
    // DATA
    GLFWwindow* d_window_p = nullptr;  // Raw GLFW handle.
    int         d_width    = 0;        // Current window width in pixels.
    int         d_height   = 0;        // Current window height in pixels.

    // PRIVATE CLASS METHODS

    /// Callback function invoked by GLFW when the framebuffer size changes.
    static void
    framebufferResizeCallback(GLFWwindow* window, int width, int height);

  public:
    // CREATORS

    /// Create a 'Window' instance. The native window is not created until
    /// 'initialize' is called.
    Window() = default;

    /// Destroy this window and terminate the GLFW context.
    ~Window() = default;

    // MANIPULATORS

    /// Create and show a native window with the specified 'title',
    /// 'requestedWidth', and 'requestedHeight'.
    void
    initialize(const char* title, int requestedWidth, int requestedHeight);

    /// Force a refresh of the cached window dimensions based on the current
    /// native window state.
    void resize() const;

    /// Process all pending system events (input, window movement, close).
    void pollEvents();

    /// Capture or release the mouse cursor within the window.
    void setMouseCapture(bool capture) const;

    /// Set the title of the native window to the specified 'title'.
    void setWindowTitle(const std::string& title) const;

    // ACCESSORS

    /// Return a pointer to the raw GLFW window handle.
    [[nodiscard]]
    GLFWwindow* window() const;

    /// Return the current width of the window in pixels.
    [[nodiscard]]
    int width() const;

    /// Return the current height of the window in pixels.
    [[nodiscard]]
    int height() const;

    /// Return 'true' if a window close event has been triggered.
    [[nodiscard]]
    bool shouldClose() const;

    /// Return 'true' if the specified GLFW 'key' is currently pressed.
    [[nodiscard]]
    bool isKeyPressed(int key) const;

    /// Return 'true' if the specified GLFW 'button' is currently pressed.
    [[nodiscard]]
    bool isMouseButtonPressed(int key) const;

    /// Return the current mouse coordinates relative to the window origin.
    [[nodiscard]]
    glm::vec2 mousePosition() const;
};

inline GLFWwindow* Window::window() const
{
    return d_window_p;
}

inline int Window::width() const
{
    return d_width;
}

inline int Window::height() const
{
    return d_height;
}

}  // close package namespace
#endif  // INCLUDED_ENG_CORE_WINDOW_H
