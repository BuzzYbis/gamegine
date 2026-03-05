// window.h                                                           -*-C++-*-
#ifndef INCLUDED_ENGINE_CORE_WINDOW_H
#define INCLUDED_ENGINE_CORE_WINDOW_H

#define GLFW_INCLUDE_VULKAN
#include <glm/vec2.hpp>

#include <GLFW/glfw3.h>
#include <string>

namespace engine::core {

class Window {
  private:
    // DATA

    GLFWwindow* d_window_p = nullptr;
    int         d_width    = 0;
    int         d_height   = 0;

    // PRIVATE CLASS METHODS

    static void
    framebufferResizeCallback(GLFWwindow* window, int width, int height);

  public:
    // CREATORS

    Window() = default;

    ~Window() = default;

    // MANIPULATORS

    void
    initialize(const char* title, int requestedWidth, int requestedHeight);

    void resize() const;

    void pollEvents();

    // ACCESSORS

    GLFWwindow* window() const { return d_window_p; }
    int         width() const { return d_width; }
    int         height() const { return d_height; }

    bool shouldClose() const;

    void setWindowTitle(const std::string& title) const;

    bool isKeyPressed(int key) const;

    bool isMouseButtonPressed(int key) const;

    void setMouseCapture(bool capture) const;

    glm::vec2 getMousePosition() const;
};

}  // close engine::core package
#endif  // INCLUDED_ENGINE_CORE_WINDOW_H
