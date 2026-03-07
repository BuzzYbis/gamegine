// vk_recource_utils.h                                                -*-C++-*-
#ifndef INCLUDED_ENGINE_RHI_VULKAN_VK_RESOURCE_UTILS_H
#define INCLUDED_ENGINE_RHI_VULKAN_VK_RESOURCE_UTILS_H

//@PURPOSE: Provide utility functions for Vulkan resource management.
//
//@CLASSES:
//  engine::rhi::vulkan::VulkanResourceUtils: Stateless utility for images and
//  command helpers.
//
//@DESCRIPTION: This component provides static helper functions to create
// images
// and their associated memory, perform common layout transitions, copy buffers
// to images, and record/submit one-time command buffers.

#include <rhi/vulkan/vk_context.h>
#include <vulkan/vulkan_raii.hpp>

namespace engine::rhi::vulkan {

struct VulkanResourceUtils {
    // ---- One-time command buffer helpers ----
    static vk::raii::CommandBuffer
    beginSingleTimeCommands(const vk::raii::Device& device,
                            vk::CommandPool         commandPool);

    static void
    endSingleTimeCommands(const vk::raii::Queue&         queue,
                          const vk::raii::CommandBuffer& commandBuffer);

    // ---- Image helpers ----
    static void createImage(const VulkanContext&    device,
                            uint32_t                width,
                            uint32_t                height,
                            uint32_t                mipLevels,
                            vk::SampleCountFlagBits numSamples,
                            vk::Format              format,
                            vk::ImageTiling         tiling,
                            vk::ImageUsageFlags     usage,
                            vk::MemoryPropertyFlags properties,
                            vk::raii::Image&        image,
                            vk::raii::DeviceMemory& imageMemory);

    static vk::raii::ImageView createImageView(
        const vk::raii::Device& device,
        const vk::raii::Image&  image,
        vk::Format              format,
        vk::ImageAspectFlags    aspectMask = vk::ImageAspectFlagBits::eColor,
        uint32_t                mipLevels  = 1);

    static void transitionImageLayout(const vk::raii::Device& device,
                                      vk::CommandPool         commandPool,
                                      const vk::raii::Queue&  queue,
                                      const vk::raii::Image&  image,
                                      vk::ImageLayout         oldLayout,
                                      vk::ImageLayout         newLayout,
                                      uint32_t                mipLevels);

    static void copyBufferToImage(const vk::raii::Device& device,
                                  vk::CommandPool         commandPool,
                                  const vk::raii::Queue&  queue,
                                  const vk::raii::Buffer& buffer,
                                  const vk::raii::Image&  image,
                                  uint32_t                width,
                                  uint32_t                height);

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
};

}  // close engine::rhi::vulkan namespace

#endif  // INCLUDED_ENGINE_RHI_VULKAN_VK_RESOURCE_UTILS_H
