// ui_manager.h                                                       -*-C++-*-
#ifndef INCLUDED_ENG_UI_UI_MANAGER_H
#define INCLUDED_ENG_UI_UI_MANAGER_H

//@PURPOSE: Provide a high-level manager for orchestrating the user interface.
//
//@CLASSES:
//  eng::ui::UIManager: Orchestrator for the UI context and UI panels.
//
//@DESCRIPTION: This component provides a centralized manager,
// 'eng::ui::UIManager', responsible for initializing the immediate-mode UI
// backend ('eng::ui::UIContext') and managing the lifecycle of various
// 'eng::ui::UIPanel' instances.
//
// By encapsulating the backend context and the collection of active panels,
// this class cleanly separates the high-level logical UI (the panels) from
// the low-level rendering mechanics.

// std
#include <memory>
#include <vector>

// rhi
#include <rhi/rhi_types.h>

// ui
#include <ui/ui_backendprotocol.h>
#include <ui/ui_context.h>
#include <ui/ui_panel.h>

namespace eng::rhi {
class SwapchainProtocol;
class ContextProtocol;
class CommandListProtocol;
}  // close package namespace

namespace eng::ui {

// ===============
// class UIManager
// ===============

/// Orchestrate the UI backend and manage the active collection of UI panels.
class UIManager {
  private:
    // DATA

    /// The technical backend handling UI state and integration.
    UIContext d_context;

    /// The collection of active logical windows/panels.
    std::vector<std::unique_ptr<UIPanel> > d_panels;

    /// The underlying API backend implementation.
    std::unique_ptr<BackendProtocol> d_backend;

  public:
    // CREATORS

    /// Create a UI manager.
    UIManager() = default;

    /// Destroy this manager and all its panels.
    ~UIManager() = default;

    // MANIPULATORS

    /// Initialize the UI backend with the provided 'context', 'swapchain',
    /// 'window' system, and 'api' type.
    void initialize(rhi::ContextProtocol*   context,
                    rhi::SwapchainProtocol* swapchain,
                    const core::Window&     window,
                    rhi::GraphicsAPI        api);

    /// Add the specified 'panel' to this manager. The manager takes ownership
    /// of the provided panel.
    void addPanel(std::unique_ptr<UIPanel> panel);

    /// Prepare the UI context for a new frame.
    void beginFrame();

    /// Iterate through all registered panels and invoke their render methods.
    void renderPanels();

    /// Finalize the UI frame and record rendering commands into the provided
    /// 'cmd' command list.
    void endFrame(rhi::CommandListProtocol* cmd);

    /// Abort the current UI frame without emitting rendering commands.
    void cleanUpFrame();

    /// Destroy the UI context and release associated resources.
    void shutdown();

    // ACCESSORS

    /// Return the underlying API backend.
    BackendProtocol* backend() const { return d_backend.get(); }
};

}  // close package namespace

#endif  // INCLUDED_ENG_UI_UI_MANAGER_H