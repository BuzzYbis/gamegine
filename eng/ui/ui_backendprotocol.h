// ui_backendprotocol.h                                               -*-C++-*-
#ifndef INCLUDED_ENG_UI_BACKENDPROTOCOL_H
#define INCLUDED_ENG_UI_BACKENDPROTOCOL_H

//@PURPOSE: Provide a protocol for UI API backend implementations.
//
//@CLASSES:
//  eng::ui::BackendProtocol: Interface for graphics API-specific UI logic.
//
//@DESCRIPTION: This component provides a pure protocol,
//'eng::ui::BackendProtocol',
// that abstracts the underlying graphics API (e.g., Vulkan, Metal) from
// the core ImGui logic. It defines the lifecycle methods required to
// initialize, update, and render UI draw data.

// ui
#include <ui/ui_panel.h>

// rhi
#include <rhi/rhi_contextprotocol.h>
#include <rhi/rhi_textureprotocol.h>

// third-party
#include <imgui.h>

// Forward declaration
struct ImDrawData;

namespace eng::ui {

// =====================
// class BackendProtocol
// =====================

/// Interface for graphics API-specific UI logic.
class BackendProtocol {
  public:
    // CREATORS

    /// Destroy this protocol.
    virtual ~BackendProtocol() = default;

    // MANIPULATORS

    /// Initialize the backend with the specified 'context' and 'swapchain'.
    virtual void initialize(rhi::ContextProtocol*   context,
                            rhi::SwapchainProtocol* swapchain) = 0;

    /// Prepare the backend for a new frame.
    virtual void newFrame() = 0;

    /// Render the specified 'drawData' using the specified 'cmd' command list.
    virtual void renderDrawData(ImDrawData*               drawData,
                                rhi::CommandListProtocol* cmd) = 0;

    /// Register the specified 'texture' and return a handle that can be used
    /// with ImGui::Image.
    virtual ImTextureID registerTexture(rhi::TextureProtocol* texture) = 0;

    /// Unregister the specified 'textureId'.
    virtual void removeTexture(ImTextureID textureId) = 0;

    /// Shutdown the backend and release associated resources.
    virtual void shutdown() = 0;
};

}  // close package namespace

#endif  // INCLUDED_ENG_UI_BACKENDPROTOCOL_H