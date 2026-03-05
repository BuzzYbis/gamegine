// ui_manager.h                                                       -*-C++-*-
#ifndef INCLUDED_ENGINE_UI_UI_MANAGER_H
#define INCLUDED_ENGINE_UI_UI_MANAGER_H

//@PURPOSE: Provide a high-level manager for orchestrating the user interface.
//
//@CLASSES:
//  engine::ui::UIManager: Orchestrator for the UI context and UI panels.
//
//@DESCRIPTION: This component provides a centralized manager,
// `engine::ui::UIManager`, responsible for initializing the immediate-mode UI
// backend (`engine::ui::UIContext`) and managing the lifecycle of various
// `engine::ui::UIPanel` instances.
//
// By encapsulating the backend context and the collection of active panels,
// this class cleanly separates the high-level logical UI (the panels) from
// the low-level rendering mechanics (Vulkan and Dear ImGui).

// ui
#include <ui/ui_context.h>
#include <ui/ui_panel.h>

// std
#include <memory>
#include <vector>

namespace engine::ui {

// ---------------
// class UIManager
// ---------------

/// Orchestrate the UI backend and manage the active collection of UI panels.
class UIManager {
  private:
    // DATA

    /// The technical backend handling Vulkan/ImGui integration.
    UIContext d_context;

    /// The collection of active logical windows/panels.
    std::vector<std::unique_ptr<UIPanel> > d_panels;

  public:
    // CREATORS

    UIManager()  = default;
    ~UIManager() = default;

    // MANIPULATORS

    /// Initialize the UI backend with the provided Vulkan context, swapchain,
    /// and window system.
    void initialize(const rhi::vulkan::VulkanContext&   context,
                    const rhi::vulkan::VulkanSwapchain& swapchain,
                    const core::Window&                 window);

    /// Add a dynamically allocated UI panel to this manager. The manager
    /// takes ownership of the provided panel.
    void addPanel(std::unique_ptr<UIPanel> panel);

    /// Prepare the UI context for a new frame (handle inputs and setup).
    void beginFrame();

    /// Iterate through all registered panels and invoke their render methods.
    void renderPanels();

    /// Finalize the UI frame and record rendering commands into the provided
    /// Vulkan command buffer.
    void endFrame(vk::CommandBuffer cmd);

    /// Abort the current UI frame without emitting rendering commands (e.g.,
    /// when the application window is minimized).
    void cleanUpFrame();

    /// Destroy the UI context and free-associated ImGui/Vulkan resources.
    void shutdown();
};

}  // close engine::ui namespace

#endif  // INCLUDED_ENGINE_UI_UI_MANAGER_H