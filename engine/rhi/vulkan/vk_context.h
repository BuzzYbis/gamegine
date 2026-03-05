// vk_context.h                                                       -*-C++-*-
#ifndef INCLUDED_ENGINE_RHI_VULKAN_VK_CONTEXT_H
#define INCLUDED_ENGINE_RHI_VULKAN_VK_CONTEXT_H

//@PURPOSE: Provide a manager for the core Vulkan objects (Instance, Device).
//
//@CLASSES:
//  engine::rhi::vulkan::VulkanContext: Main entry point for Vulkan state.
//
//@DESCRIPTION: This component implements the initialization and storage of
// the fundamental Vulkan objects, including the Instance, PhysicalDevice,
// LogicDevice, and queues. It manages the lifetime of these objects using
// RAII wrappers from vulkan.hpp.

// core
#include <core/window.h>

// vulkan
#include <vulkan/vulkan_raii.hpp>

namespace engine::rhi::vulkan {

// -------------------
// class VulkanContext
// -------------------

/// This class encapsulates the entire Vulkan context state. It manages the
/// bootstrap process (Instance -> Surface -> PhysicalDevice -> LogicalDevice)
/// and holds the ownership of these fundamental resources.
class VulkanContext {
  private:
    // DATA

    /// Object responsible for loading global Vulkan function pointers.
    /// It acts as the factory for the `vk::raii::Instance`.
    vk::raii::Context d_context;

    /// The root handle to the Vulkan library. It isolates the application
    /// state from other applications.
    vk::raii::Instance d_instance = nullptr;

    /// Handle to the chosen graphics card (GPU).
    vk::raii::PhysicalDevice d_physicalDevice = nullptr;

    /// The logical interface to the physical device. This is the primary
    /// object used to create resources (Buffers, Images, Pipelines).
    vk::raii::Device d_device = nullptr;

    /// Interface to the execution queue on the device. Commands recorded in
    /// command buffers are submitted to this queue for execution.
    vk::raii::Queue d_graphicsQueue = nullptr;

    /// Abstraction of the native window surface (WSI). Used by the Swapchain
    /// to present images to the screen.
    vk::raii::SurfaceKHR d_surface = nullptr;

    /// Pointer to the platform window manager.
    /// Owned by the Engine, not by VulkanContext.
    core::Window* d_window_p = nullptr;

    /// The selected Multi-Sample Anti-Aliasing count (e.g., 1, 2, 4, 8).
    vk::SampleCountFlagBits d_msaaSamples = vk::SampleCountFlagBits::e1;

    /// The index of the queue family used for graphics and presentation.
    uint32_t d_graphicsQueueFamilyIndex = 0;

    // PRIVATE MANIPULATORS

    /// Initialize the Vulkan library, load required extensions (including
    /// specific platform surface extensions), and enable validation layers if
    /// requested.
    bool createInstance(bool enableValidation);

    /// Create the window surface (WSI) using the Platform abstraction.
    /// This bridges the OS windowing system (Cocoa, Win32, X11) with Vulkan.
    bool createSurface();

    /// Iterate over available GPUs and select the most suitable one based on
    /// feature support (Geometry Shader, Anisotropy, etc.) and presentation
    /// support.
    bool pickPhysicalDevice();

    /// Iterate over the queue families of the physical device to find one that
    /// supports both Graphics operations (`VK_QUEUE_GRAPHICS_BIT`) and
    /// Presentation to the created surface.
    bool findGraphicsQueueFamily();

    /// Create the logical device interface and the command queues.
    /// This step also enables device-specific features (like Dynamic
    /// Rendering).
    bool createLogicalDevice();

    // PRIVATE ACCESSORS

    /// Calculate the maximum MSAA sample count supported by both the Color
    /// and Depth attachments of the physical device.
    [[nodiscard]] vk::SampleCountFlagBits findMaxUsableSampleCount() const;

  public:
    // CREATORS

    /// Create a Vulkan context associated with the given platform window.
    /// The platform pointer must remain valid during the lifetime of this
    /// context.
    explicit VulkanContext(core::Window* platform);

    /// Destroy the Vulkan context and release all associated GPU resources.
    /// Resources are destroyed in the reverse order of their creation due to
    /// RAII.
    ~VulkanContext() = default;

    // MANIPULATORS

    /// Initialize the Vulkan instance, surface, physical device selection, and
    /// logical device creation. Returns false if any step of the
    /// initialization fails (errors are logged to stderr).
    [[nodiscard]] bool initialize(bool enableValidation);

    // ACCESSORS

    /// Return the Vulkan Instance (connection to the driver).
    [[nodiscard]] const vk::raii::Instance& instance() const;

    /// Return the selected Physical Device (The GPU hardware).
    [[nodiscard]] const vk::raii::PhysicalDevice& physicalDevice() const;

    /// Return the Logical Device (The software interface to the GPU).
    [[nodiscard]] const vk::raii::Device& device() const;

    /// Return the Graphics Queue used for submitting command buffers.
    [[nodiscard]] const vk::raii::Queue& graphicsQueue() const;

    /// Return the MSAA (Multi-Sample Anti-Aliasing) sample count supported
    /// and selected for this context.
    [[nodiscard]] vk::SampleCountFlagBits msaaSamples() const;

    /// Return the index of the queue family that supports graphics operations
    /// and presentation to the surface.
    [[nodiscard]] uint32_t graphicsQueueFamilyIndex() const;

    /// Return the platform abstraction used to create the window surface.
    [[nodiscard]] core::Window* window() const;

    /// Return a const reference to the window surface.
    [[nodiscard]] const vk::raii::SurfaceKHR& surface() const;
};

// =======================================================================
//                          INLINE DEFINITIONS
// =======================================================================

// ACCESSORS

inline const vk::raii::Instance& VulkanContext::instance() const
{
    return d_instance;
}

inline const vk::raii::PhysicalDevice& VulkanContext::physicalDevice() const
{
    return d_physicalDevice;
}

inline const vk::raii::Device& VulkanContext::device() const
{
    return d_device;
}

inline const vk::raii::Queue& VulkanContext::graphicsQueue() const
{
    return d_graphicsQueue;
}

inline vk::SampleCountFlagBits VulkanContext::msaaSamples() const
{
    return d_msaaSamples;
}

inline uint32_t VulkanContext::graphicsQueueFamilyIndex() const
{
    return d_graphicsQueueFamilyIndex;
}

inline core::Window* VulkanContext::window() const
{
    return d_window_p;
}

inline const vk::raii::SurfaceKHR& VulkanContext::surface() const
{
    return d_surface;
}

}  // close engine::rhi::vulkan namespace

#endif  // INCLUDED_ENGINE_RHI_VULKAN_VK_CONTEXT_H
