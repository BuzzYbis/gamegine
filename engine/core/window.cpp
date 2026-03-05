// window.cpp.                                                        -*-C++-*-
#include <core/window.h>
#include <iostream>

namespace engine::core {
void Window::initialize(const char* title,
                        const int   requestedWidth,
                        const int   requestedHeight)
{
    // Initialize GLFW library
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    // By default, glfw was created for OpenGL since we used vulkan we need to
    // disable OpenGL context creation
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // Enable window resizing
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    // Create GLFW window
    d_window_p = glfwCreateWindow(requestedWidth,
                                  requestedHeight,
                                  title,
                                  nullptr,
                                  nullptr);

    // Check if the window is created
    if (!d_window_p) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    // Retrieve windows size
    glfwGetFramebufferSize(d_window_p, &d_width, &d_height);

    // Set the user pointer to the platform instance
    glfwSetWindowUserPointer(d_window_p, this);

    glfwSetFramebufferSizeCallback(d_window_p, framebufferResizeCallback);
}

void Window::resize() const
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(d_window_p, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(d_window_p, &width, &height);
        glfwWaitEvents();
    }
}

bool Window::shouldClose() const
{
    return glfwWindowShouldClose(d_window_p);
}

void Window::pollEvents()
{
    glfwPollEvents();
}

void Window::setWindowTitle(const std::string& title) const
{
    glfwSetWindowTitle(d_window_p, title.c_str());
}

void Window::framebufferResizeCallback(GLFWwindow* window,
                                       const int   width,
                                       const int   height)
{
    const auto app = static_cast<Window*>(glfwGetWindowUserPointer(window));
    app->d_width   = width;
    app->d_height  = height;
}

bool Window::isKeyPressed(const int key) const
{
    return glfwGetKey(d_window_p, key) == GLFW_PRESS;
}

bool Window::isMouseButtonPressed(const int key) const
{
    return glfwGetMouseButton(d_window_p, key) == GLFW_PRESS;
}

void Window::setMouseCapture(const bool capture) const
{
    if (capture) {
        glfwSetInputMode(d_window_p, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        // On reset la position pour éviter un saut de caméra à l'activation
        if (glfwRawMouseMotionSupported())
            glfwSetInputMode(d_window_p, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
    else {
        glfwSetInputMode(d_window_p, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

glm::vec2 Window::getMousePosition() const
{
    double x, y;
    glfwGetCursorPos(d_window_p, &x, &y);
    return {static_cast<float>(x), static_cast<float>(y)};
}

}