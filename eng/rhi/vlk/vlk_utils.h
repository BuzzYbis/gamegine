// vlk_utils.h                                                         -*-C++-*-
#ifndef INCLUDED_ENG_RHI_VLK_UTILS_H
#define INCLUDED_ENG_RHI_VLK_UTILS_H

//@PURPOSE: Provide internal utilities for the Vulkan RHI implementation.
//
//@CLASSES:
//  eng::rhi::vlk::Utils: Internal Vulkan-specific helper functions.
//
//@DESCRIPTION: This component provides a suite of static utility functions
// used internally by the Vulkan RHI backend to simplify repetitive tasks
// such as image creation, memory type lookup, and layout transitions.
// These functions are NOT part of the public RHI interface.

// vlk
#include <rhi/vlk/vlk_context.h>

// vulkan
#include <vulkan/vulkan_raii.hpp>

namespace eng::rhi::vlk {

// ============
// struct Utils
// ============

struct Utils {
    // CLASS METHODS

    /// Return the memory type index that supports the specified 'typeFilter'
    /// and 'properties' for the specified 'physicalDevice'.
    static uint32_t findMemoryTypeIndex(
        const vk::raii::PhysicalDevice& physicalDevice,
        uint32_t                        typeFilter,
        vk::MemoryPropertyFlags         properties);

    /// Create a Vulkan image and its associated memory with the specified
    /// configuration using the specified 'context'.
    static void createImage(Context*                context,
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

    /// Create and return a Vulkan image view for the specified 'image' with
    /// the specified 'format' and 'aspectMask'.
    static vk::raii::ImageView createImageView(
        const vk::raii::Device&    device,
        const vk::raii::Image&     image,
        vk::Format                 format,
        vk::ImageAspectFlags       aspectMask,
        uint32_t                   mipLevels = 1);

    /// Transition the specified 'image' from 'oldLayout' to 'newLayout' using
    /// the specified 'commandBuffer'.
    static void transitionImageLayout(
        const vk::raii::CommandBuffer& commandBuffer,
        const vk::raii::Image&         image,
        vk::ImageLayout                oldLayout,
        vk::ImageLayout                newLayout,
        uint32_t                       mipLevels = 1);

    /// Return the first format in the specified 'candidates' that supports
    /// the specified 'tiling' and 'features' for the specified 'physicalDevice'.
    static vk::Format findSupportedFormat(
        const vk::raii::PhysicalDevice& physicalDevice,
        const std::vector<vk::Format>&  candidates,
        vk::ImageTiling                 tiling,
        vk::FormatFeatureFlags          features);

    /// Return an optimal depth format supported by the specified 'physicalDevice'.
    static vk::Format findDepthFormat(const vk::raii::PhysicalDevice& physicalDevice);

    /// Return the Vulkan format corresponding to the specified RHI 'format'.
    static vk::Format getVkFormat(rhi::Format format);
};

}  // close package namespace

#endif  // INCLUDED_ENG_RHI_VLK_UTILS_H
