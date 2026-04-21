// ui_context.h                                                       -*-C++-*-
#ifndef INCLUDED_ENG_UI_UI_CONTEXT_H
#define INCLUDED_ENG_UI_UI_CONTEXT_H

//@PURPOSE: Provide a wrapper for Dear ImGui.
//
//@CLASSES:
//  eng::ui::UIContext: Wrapper for Dear ImGui lifecycle and state.
//
//@DESCRIPTION: This component provides a class, 'eng::ui::UIContext', that
// manages the initialization, frame lifecycle, and destruction of a
// Dear ImGui context. It acts as the bridge between the high-level UI
// manager and the low-level API backends.

// std
#include <memory>

// third-party
#include <imgui.h>

namespace eng::core {

class Window;

}  // close package namespace

namespace eng::rhi {

class CommandListProtocol;
class SwapchainProtocol;
class ContextProtocol;

}  // close package namespace

namespace eng::ui {

class BackendProtocol;

// ===============
// class UIContext
// ===============

/// Wrapper for Dear ImGui lifecycle and state.
class UIContext {
  private:
    // DATA

    /// Hold the context of the Dear ImGui
    ImGuiContext* d_context_p = nullptr;

    /// Non-owning pointer to the current backend implementation.
    BackendProtocol* d_backend_p = nullptr;

  public:
    // CREATORS

    /// Create a UI context.
    UIContext() = default;

    /// Destroy this UI context.
    ~UIContext() = default;

    // MANIPULATORS

    /// Connects ImGui to the windowing system (GLFW) and rendering system
    /// (Vulkan) via the specified 'backend'.
    void initialize(rhi::ContextProtocol*                   context,
                    rhi::SwapchainProtocol*                 swapchain,
                    const core::Window&                     platform,
                    const std::unique_ptr<BackendProtocol>& backend);

    /// This function need to be called in the beginning of the main loop.
    void beginFrame();

    /// Ends the ImGui frame without generating rendering commands.
    /// Note that this is automatically called internally by `endFrame`
    /// (via `ImGui::Render()`). You only need to call `cleanUpFrame` manually
    /// if you started a frame with `beginFrame` but decided to abort it
    /// and skip rendering entirely (e.g., when the application window is
    /// resized).
    void cleanUpFrame();

    /// Once we have defined the entire user interface in the main loop, we
    /// call this function to generate all the rendering commands. This
    /// function takes a 'cmd' parameter that must be in record mode.
    void endFrame(rhi::CommandListProtocol* cmd);

    /// When the engine is shut down, the destruction order is the reverse of
    /// the initialization order.
    void shutdown();
};

}  // close package namespace

#endif  // INCLUDED_ENG_UI_UI_CONTEXT_H
