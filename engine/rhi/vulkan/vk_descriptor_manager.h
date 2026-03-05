// VulkanDescriptorManager.hpp                                        -*-C++-*-
#ifndef INCLUDED_CORE_VULKANDESCRIPTORMANAGER
#define INCLUDED_CORE_VULKANDESCRIPTORMANAGER

//@PURPOSE: Provide a manager for Vulkan Descriptor Sets and Uniform Buffers.
//
//@CLASSES:
//  example::VulkanDescriptorManager: RAII mechanism for descriptor resources.
//
//@DESCRIPTION: This component manages the lifecycle of 'vk::DescriptorPool',
// 'vk::DescriptorSet', and the underlying 'vk::Buffer' (Uniform Buffers). It
// handles the allocation of memory, mapping to the CPU for updates, and
// ensures synchronization for multi-buffering by duplicating resources per
// swapchain image.

#include <rhi/vulkan/vk_swapchain.h>
#include <vector>
#include <unordered_map>
#include <vulkan/vulkan_raii.hpp>

namespace engine::rhi::vulkan {

// -----------------------------
// class VulkanDescriptorManager
// -----------------------------

// This class is responsible for managing the lifecycle of Vulkan Descriptor
// Sets and Uniform Buffers.
class DescriptorManager {
  private:
    // DATA

    VulkanContext&                       d_context;
    VulkanSwapchain&                     m_swapchain;
    const vk::raii::DescriptorSetLayout& m_descriptorSetLayout;

    vk::raii::DescriptorPool             m_descriptorPool = nullptr;
    
    struct DescriptorSetKey {
        VkImageView imageView;
        VkSampler   sampler;

        bool operator==(const DescriptorSetKey& other) const
        {
            return imageView == other.imageView && sampler == other.sampler;
        }
    };

    struct KeyHash {
        std::size_t operator()(const DescriptorSetKey& k) const
        {
            return std::hash<VkImageView>{}(k.imageView) ^
                   (std::hash<VkSampler>{}(k.sampler) << 1);
        }
    };

    mutable std::unordered_map<DescriptorSetKey,
                               std::vector<vk::raii::DescriptorSet>,
                               KeyHash>
        m_descriptorSetsCache;

    std::vector<vk::raii::Buffer>       m_uniformBuffers;
    std::vector<vk::raii::DeviceMemory> m_uniformBuffersMemory;
    std::vector<void*>                  m_uniformBuffersMapped;
    vk::DeviceSize                      m_uniformBufferSize;

  public:
    // CREATORS

    //// Create a `DescriptorManager` having `uniformBufferSize` bytes for each
    /// swapchain image.
    DescriptorManager(VulkanContext&                       context,
                      VulkanSwapchain&                     swapchain,
                      const vk::raii::DescriptorSetLayout& descriptorSetLayout,
                      size_t                               uniformBufferSize);

    // ACCESSORS

    //// Update the uniform buffer for the given `imageIndex` with the given
    ///`data`.
    void updateUniformBuffer(uint32_t imageIndex, const void* data) const;

    //// Return the descriptor set for the given `imageIndex` and texture.
    const vk::raii::DescriptorSet&
    getDescriptorSet(uint32_t                imageIndex,
                     const vk::raii::ImageView& imageView,
                     const vk::raii::Sampler&   sampler);

    bool hasDescriptorSets() const { return true; }

  private:
    // PRIVATE MANIPULATORS

    void createDescriptorPool();
    void createUniformBuffers(size_t bufferSize);
};
}  // close namespace core

#endif  // INCLUDED_CORE_VULKANDESCRIPTORMANAGER