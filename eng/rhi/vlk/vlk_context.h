// vlk_context.h                                                      -*-C++-*-
#ifndef INCLUDED_ENG_RHI_VLK_CONTEXT_H
#define INCLUDED_ENG_RHI_VLK_CONTEXT_H

//@PURPOSE: Provide a manager for the core Vulkan objects (Instance, Device).
//
//@CLASSES:
//  eng::rhi::vlk::Context: Main entry point for Vulkan state.
//
//@DESCRIPTION: This component provides a class, 'eng::rhi::vlk::Context',
// that implements the initialization and storage of the fundamental Vulkan
// objects, including the Instance, PhysicalDevice, LogicalDevice, and queues.
// It manages the lifetime of these objects using RAII wrappers from
// vulkan.hpp.

// core
#include <core/core_window.h>

// rhi
#include <rhi/rhi_contextprotocol.h>

// vulkan
#include <vulkan/vulkan_raii.hpp>

// Forward declarations
namespace eng::rhi {
class CommandListProtocol;
}  // close package namespace

namespace eng::rhi::vlk {

// =============
// class Context
// =============

/// This class implements the 'eng::rhi::ContextProtocol' for the Vulkan
/// backend. It encapsulates the global Vulkan state and serves as the primary
/// factory for all Vulkan-specific RHI resources.
class Context : public ContextProtocol {
  private:
    // DATA

    /// Object responsible for loading global Vulkan function pointers.
    /// It acts as the factory for the 'vk::raii::Instance'.
    vk::raii::Context d_context;

    /// The root handle to the Vulkan library. It isolates the application
    /// state from other applications.
    vk::raii::Instance d_instance;

    /// Handle to the chosen graphics card (GPU).
    vk::raii::PhysicalDevice d_physicalDevice;

    /// The logical interface to the physical device. This is the primary
    /// object used to create resources (Buffers, Images, Pipelines).
    vk::raii::Device d_device;

    /// Interface to the execution queue on the device. Commands recorded in
    /// command buffers are submitted to this queue for execution.
    vk::raii::Queue d_graphicsQueue;

    /// Abstraction of the native window surface (WSI). Used by the Swapchain
    /// to present images to the screen.
    vk::raii::SurfaceKHR d_surface;

    /// Pointer to the platform window manager.
    /// Owned by the Engine, not by this 'Context'.
    core::Window* d_window_p;

    /// The selected Multi-Sample Anti-Aliasing count (e.g., 1, 2, 4, 8).
    vk::SampleCountFlagBits d_msaaSamples;

    /// The index of the queue family used for graphics and presentation.
    uint32_t d_graphicsQueueFamilyIndex;

    /// The command pool for the graphics queue.
    vk::raii::CommandPool d_commandPool;

    /// The descriptor pool for the graphics queue.
    vk::raii::DescriptorPool d_descriptorPool;

    // PRIVATE MANIPULATORS

    /// Initialize the Vulkan library, load required extensions (including
    /// specific platform surface extensions), and enable validation layers if
    /// the specified 'enableValidation' is 'true'. Return 'true' on success.
    bool createInstance(bool enableValidation);

    /// Create the window surface (WSI) using the Platform abstraction.
    /// Return 'true' on success, and 'false' otherwise.
    bool createSurface();

    /// Iterate over available GPUs and select the most suitable one based on
    /// feature support and presentation capabilities. Return 'true' on
    /// success.
    bool pickPhysicalDevice();

    /// Iterate over the queue families of the physical device to find one that
    /// supports both Graphics operations and Presentation to the surface.
    /// Return 'true' on success.
    bool findGraphicsQueueFamily();

    /// Create the logical device interface and the command queues.
    /// Return 'true' on success.
    bool createLogicalDevice();

    // PRIVATE ACCESSORS

    /// Return the maximum MSAA sample count supported by both the Color
    /// and Depth attachments of the physical device.
    [[nodiscard]]
    vk::SampleCountFlagBits findMaxUsableSampleCount() const;

  public:
    // CREATORS

    /// Create a Vulkan context associated with the specified 'platform'
    /// window. The behavior is undefined unless 'platform' remains valid for
    /// the lifetime of this object.
    explicit Context(core::Window* platform);

    /// Destroy this Vulkan context and release all associated GPU resources.
    ~Context() override = default;

    // MANIPULATORS

    /// Initialize the Vulkan instance, surface, physical device selection, and
    /// logical device creation. Enable validation layers if the specified
    /// 'enableValidation' is 'true'. Return 'true' on success, and 'false'
    /// if any step of the initialization fails.
    [[nodiscard]]
    bool initialize(bool enableValidation) override;

    /// Block the calling thread until the GPU has finished executing all
    /// submitted commands.
    void waitIdle() override;

    /// Create and return a newly allocated swapchain configured with the
    /// specified 'width' and 'height'.
    std::unique_ptr<SwapchainProtocol>
    createSwapchain(uint32_t width, uint32_t height) override;

    /// Create and return a newly allocated command list.
    std::unique_ptr<CommandListProtocol> createCommandList() override;

    /// Create and return a newly allocated buffer of the specified 'size'
    /// in bytes, configured for the specified 'usage'.
    std::unique_ptr<BufferProtocol> createBuffer(size_t      size,
                                                 BufferUsage usage) override;

    /// Create and return a newly allocated graphics pipeline configured
    /// according to the specified 'config'.
    std::unique_ptr<PipelineProtocol>
    createPipeline(const PipelineConfig& config) override;

    /// Create and return a newly allocated texture with the specified
    /// 'width', 'height', 'mipLevels', 'format', and 'pixels' data.
    std::unique_ptr<TextureProtocol>
    createTexture(uint32_t    width,
                  uint32_t    height,
                  uint32_t    mipLevels,
                  Format      format,
                  const void* pixels) override;

    /// Create and return a newly allocated resource layout configured
    /// according to the specified 'config'.
    std::unique_ptr<ResourceLayoutProtocol>
    createResourceLayout(const ResourceLayoutConfig& config) override;

    /// Create and return a newly allocated resource set bound to the
    /// specified 'layout'. The behavior is undefined if 'layout' is null.
    std::unique_ptr<ResourceSetProtocol>
    createResourceSet(ResourceLayoutProtocol* layout) override;

    // ACCESSORS

    /// Return a const reference to the Vulkan Instance.
    [[nodiscard]]
    const vk::raii::Instance& instance() const;

    /// Return a const reference to the selected Physical Device.
    [[nodiscard]]
    const vk::raii::PhysicalDevice& physicalDevice() const;

    /// Return a const reference to the Logical Device.
    [[nodiscard]]
    const vk::raii::Device& device() const;

    /// Return a const reference to the Graphics Queue.
    [[nodiscard]]
    const vk::raii::Queue& graphicsQueue() const;

    /// Return the MSAA (Multi-Sample Anti-Aliasing) sample count selected
    /// for this context.
    [[nodiscard]]
    vk::SampleCountFlagBits msaaSamples() const;

    /// Return the index of the queue family that supports graphics operations
    /// and presentation.
    [[nodiscard]]
    uint32_t graphicsQueueFamilyIndex() const;

    /// Return the platform window abstraction associated with this context.
    [[nodiscard]]
    core::Window* window() const;

    /// Return a const reference to the window surface.
    [[nodiscard]]
    const vk::raii::SurfaceKHR& surface() const;

    /// Return a const reference to the command pool.
    [[nodiscard]]
    const vk::raii::CommandPool& commandPool() const;

    /// Return a const reference to the descriptor pool.
    [[nodiscard]]
    const vk::raii::DescriptorPool& descriptorPool() const;
};

// =======================================================================
//                          INLINE DEFINITIONS
// =======================================================================

// ACCESSORS

inline const vk::raii::Instance& Context::instance() const
{
    return d_instance;
}

inline const vk::raii::PhysicalDevice& Context::physicalDevice() const
{
    return d_physicalDevice;
}

inline const vk::raii::Device& Context::device() const
{
    return d_device;
}

inline const vk::raii::Queue& Context::graphicsQueue() const
{
    return d_graphicsQueue;
}

inline vk::SampleCountFlagBits Context::msaaSamples() const
{
    return d_msaaSamples;
}

inline uint32_t Context::graphicsQueueFamilyIndex() const
{
    return d_graphicsQueueFamilyIndex;
}

inline core::Window* Context::window() const
{
    return d_window_p;
}

inline const vk::raii::SurfaceKHR& Context::surface() const
{
    return d_surface;
}

inline const vk::raii::CommandPool& Context::commandPool() const
{
    return d_commandPool;
}

inline const vk::raii::DescriptorPool& Context::descriptorPool() const
{
    return d_descriptorPool;
}

}  // close package namespace
#endif  // INCLUDED_ENG_RHI_VLK_CONTEXT_H
