// ui_vulkanbackend.h                                                 -*-C++-*-
#ifndef INCLUDED_ENG_UI_UI_VULKANBACKEND_H
#define INCLUDED_ENG_UI_UI_VULKANBACKEND_H

//@PURPOSE: Provide a Vulkan implementation of the UI backend protocol.
//
//@CLASSES:
//  eng::ui::VulkanBackend: Vulkan-specific implementation of BackendProtocol.
//
//@DESCRIPTION: This component provides a class, 'eng::ui::VulkanBackend', that
// implements the 'eng::ui::BackendProtocol' using the Vulkan graphics API.
// It handles the initialization of Dear ImGui's Vulkan backend and manages
// the necessary Vulkan resources, such as the descriptor pool.

// ui
#include <ui/ui_backendprotocol.h>

// rhi
#include <rhi/vlk/vlk_context.h>

// vulkan
#include <imgui_impl_vulkan.h>

namespace eng::ui {

// ===================
// class VulkanBackend
// ===================

/// Vulkan-specific implementation of the UI backend protocol.
class VulkanBackend : public BackendProtocol {
  private:
    // DATA
    vk::raii::DescriptorPool d_descriptorPool = nullptr;

  public:
    // CREATORS

    /// Create a Vulkan UI backend.
    VulkanBackend() = default;

    /// Destroy this backend and release associated Vulkan resources.
    ~VulkanBackend() override = default;

    // MANIPULATORS

    /// Initialize the Vulkan backend with the specified 'context' and
    /// 'swapchain'.
    void initialize(rhi::ContextProtocol*   context,
                    rhi::SwapchainProtocol* swapchain) override;

    /// Prepare the backend for a new frame.
    void newFrame() override;

    /// Render the specified 'drawData' using the specified 'cmd' command list.
    void renderDrawData(ImDrawData*               drawData,
                        rhi::CommandListProtocol* cmd) override;

    /// Register the specified 'texture' and return a handle that can be used
    /// with ImGui::Image.
    ImTextureID registerTexture(rhi::TextureProtocol* texture) override;

    /// Unregister the specified 'textureId'.
    void removeTexture(ImTextureID textureId) override;

    /// Shutdown the backend and release all resources.
    void shutdown() override;
};

}  // close package namespace

#endif  // INCLUDED_ENG_UI_UI_VULKANBACKEND_H
