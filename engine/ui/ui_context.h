// ui_context.h                                                       -*-C++-*-
#ifndef INCLUDED_ENGINE_UI_UI_CONTEXT_H
#define INCLUDED_ENGINE_UI_UI_CONTEXT_H

//@PURPOSE: Provide a wrapper for Dear ImGui.
//
//@CLASSES:
//  engine::ui::UIContext: Wrapper for Dear ImGui.
//
//@DESCRIPTION: This component provides a wrapper for Dear ImGui. It manages
// the initialization, rendering loop, and destruction of the ImGui context.
// Note that this wrapper is strictly coupled to the Vulkan backend and GLFW.

// imgui
#include <imgui.h>

// rhi
#include <rhi/vulkan/vk_context.h>
#include <rhi/vulkan/vk_swapchain.h>

// vulkan
#include <vulkan/vulkan_raii.hpp>

namespace engine::ui {

// ---------------
// class UIContext
// ---------------

/// Wrapper for Dear ImGui.
class UIContext {
  private:
    // DATA

    /// RAII descriptor pool for ImGui; must outlive ImGui_ImplVulkan_* calls.
    /// Storing vk::DescriptorPool (raw handle) caused use-after-free: the
    /// temporary raii object was destroyed, invalidating the handle.
    vk::raii::DescriptorPool d_descriptorPool = nullptr;

    /// Hold the context of the Dear ImGui
    ImGuiContext* d_context_p = nullptr;

  public:
    // MANIPULATORS

    /// Connects ImGui to the windowing system (GLFW) and rendering system
    /// (Vulkan).
    void initialize(const rhi::vulkan::VulkanContext&   context,
                    const rhi::vulkan::VulkanSwapchain& swapchain,
                    const core::Window&                 platform);

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
    /// function takes a `vk::CommandBuffer` parameter that must be in record
    /// mode.
    void endFrame(vk::CommandBuffer cmd);

    /// When the engine is shut down, the destruction order is the reverse of
    /// the initialization order.
    void shutdown();
};

}  // close engine::ui namespace

#endif  // INCLUDED_ENGINE_UI_UI_CONTEXT_H
