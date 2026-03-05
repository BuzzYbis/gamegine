// vk_buffer.h                                                        -*-C++-*-
#ifndef INCLUDED_ENGINE_RHI_VULKAN_VK_BUFFER_H
#define INCLUDED_ENGINE_RHI_VULKAN_VK_BUFFER_H

//@PURPOSE: Provide utility functions for Vulkan buffer management.
//
//@CLASSES:
//  core::VulkanBuffer: Stateless utility for buffers and memory.
//
//@DESCRIPTION: This component provides static helper functions to find
// suitable memory types on the physical device and to create/allocate
// Vulkan buffers in a single call.

// rhi
#include <rhi/vulkan/vk_context.h>

// vulkan
#include <vulkan/vulkan_raii.hpp>

namespace engine::rhi::vulkan {

///
struct VulkanBuffer {
    //// Find a memory type index that supports the given `typeFilter` and
    //// `properties` on the specified `device`.
    static uint32_t findMemoryTypeIndex(const VulkanContext&    device,
                                        uint32_t                typeFilter,
                                        vk::MemoryPropertyFlags properties);

    //// Create a buffer and its associated memory. Return a pair containing
    //// the buffer and its device memory (RAII).
    static void createBuffer(const VulkanContext&    device,
                             vk::DeviceSize          size,
                             vk::BufferUsageFlags    usage,
                             vk::MemoryPropertyFlags properties,
                             vk::raii::Buffer&       buffer,
                             vk::raii::DeviceMemory& memory);
};

}  // close engine::rhi::vulkan namespace

#endif  // INCLUDED_ENGINE_RHI_VULKAN_VK_BUFFER_H
