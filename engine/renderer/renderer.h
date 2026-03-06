// renderer.h                                                         -*-C++-*-
#ifndef INCLUDED_ENGINE_RENDERER_RENDERER_H
#define INCLUDED_ENGINE_RENDERER_RENDERER_H

//@PURPOSE: Provide the main rendering orchestration subsystem.
//
//@CLASSES:
//  engine::renderer::Renderer: Manages Vulkan state and frame drawing.
//
//@DESCRIPTION: This component is responsible for orchestrating the drawing
// of frames using Vulkan. It abstracts the synchronization (Fences,
// Semaphores) and command buffer management away from the main game loop.

// core
#include <core/window.h>

// renderer
#include <renderer/command_pool.h>

// rhi
#include <rhi/vulkan/vk_context.h>
#include <rhi/vulkan/vk_descriptor_manager.h>
#include <rhi/vulkan/vk_pipeline.h>
#include <rhi/vulkan/vk_swapchain.h>

// scene
#include <scene/scene.h>

// vulkan
#include <vulkan/vulkan_raii.hpp>

// std
#include <memory>
#include <vector>

struct RenderTarget {
    vk::raii::Image        image   = nullptr;
    vk::raii::DeviceMemory memory  = nullptr;
    vk::raii::ImageView    view    = nullptr;
    vk::raii::Sampler      sampler = nullptr;

    vk::raii::Image        msaaImage  = nullptr;
    vk::raii::DeviceMemory msaaMemory = nullptr;
    vk::raii::ImageView    msaaView   = nullptr;

    vk::Format   format = vk::Format::eR8G8B8A8Unorm;
    vk::Extent2D extent;
};

namespace engine::renderer {

class Texture;  // Forward declaration

class Renderer {
  private:
    // DATA
    core::Window* d_window_p = nullptr;

    std::unique_ptr<rhi::vulkan::VulkanContext>     d_context;
    std::unique_ptr<rhi::vulkan::VulkanSwapchain>   d_swapchain;
    std::unique_ptr<rhi::vulkan::DescriptorManager> d_descriptorManager;
    std::unique_ptr<CommandPool>                    d_commandPool;

    RenderTarget d_renderTarget;

    vk::raii::DescriptorSetLayout d_descriptorSetLayout = nullptr;
    vk::raii::CommandBuffers      d_commandBuffers;

    // Synchronization objects
    std::vector<vk::raii::Semaphore> d_imageAvailableSemaphores;
    std::vector<vk::raii::Semaphore> d_renderFinishedSemaphores;
    std::vector<vk::raii::Fence>     d_inFlightFences;

    uint32_t d_currentFrame = 0;  // CPU frame index (for sync objects)
    uint32_t d_imageIndex   = 0;  // GPU image index (from swapchain)

    std::unordered_map<std::string,
                       std::unique_ptr<rhi::vulkan::VulkanPipeline> >
        d_pipelines;

  private:
    // PRIVATE MANIPULATORS

    // Transitions an image from one layout to another using memory barriers.
    static void transition_image_layout(const vk::CommandBuffer& commandBuffer,
                                        const vk::Image&         image,
                                        vk::ImageLayout          old_layout,
                                        vk::ImageLayout          new_layout,
                                        vk::AccessFlags2 src_access_mask,
                                        vk::AccessFlags2 dst_access_mask,
                                        vk::PipelineStageFlags2 src_stage_mask,
                                        vk::PipelineStageFlags2 dst_stage_mask,
                                        vk::ImageAspectFlags    aspect_flags);

    void createPipelines();

  public:
    // CREATORS

    explicit Renderer(core::Window* platform);
    ~Renderer();

    // MANIPULATORS

    bool initialize(bool enableValidationLayers);

    /// Starts the frame, waits for fences, acquires the next image, and begins
    /// recording the command buffer. Returns the command buffer, or nullptr if
    /// the frame was skipped (e.g., window minimized).
    vk::CommandBuffer beginFrame(scene::Scene& scene);

    /// Ends dynamic rendering, finishes the command buffer recording, submits
    /// it to the GPU, and presents the image to the screen.
    void endFrame(vk::CommandBuffer cmd);

    void createSyncObjects();

    void createRenderTarget();

    void beginSwapchainPass(vk::CommandBuffer cmd);

    [[nodiscard]] rhi::vulkan::VulkanPipeline* getPipeline(const std::string& name) const;

    // ACCESSORS

    /// Iterates over the scene ECS and records draw commands into the given
    /// command buffer using Dynamic Rendering.
    void renderScene(vk::CommandBuffer cmd, scene::Scene& scene) const;

    /// Updates the global uniform buffer (e.g., camera matrices).
    void updateUniformBuffer(uint32_t currentImage, scene::Scene& scene) const;

    /// Returns a reference to the underlying Vulkan context.
    rhi::vulkan::VulkanContext& context() const;

    /// Returns a pointer to the swapchain.
    rhi::vulkan::VulkanSwapchain* swapchain() const;

    vk::CommandPool commandPool() const;

    const vk::raii::DescriptorSetLayout& descriptorSetLayout() const;

    vk::ImageView renderTargetView() const;
    vk::Sampler   renderTargetSampler() const;
};

inline rhi::vulkan::VulkanContext& Renderer::context() const
{
    return *d_context;
}

inline rhi::vulkan::VulkanSwapchain* Renderer::swapchain() const
{
    return d_swapchain.get();
}

inline vk::CommandPool Renderer::commandPool() const
{
    return *d_commandPool->commandPool();
}

inline const vk::raii::DescriptorSetLayout&
Renderer::descriptorSetLayout() const
{
    return d_descriptorSetLayout;
}

inline vk::ImageView Renderer::renderTargetView() const
{
    return d_renderTarget.view;
}

inline vk::Sampler Renderer::renderTargetSampler() const
{
    return d_renderTarget.sampler;
}

}  // close engine::renderer namespace

#endif  // INCLUDED_ENGINE_RENDERER_RENDERER_H