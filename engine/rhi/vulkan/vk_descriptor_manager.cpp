#include "rhi/vulkan/vk_descriptor_manager.h"
#include "rhi/vulkan/vk_swapchain.h"

#include <array>
#include <vulkan/vulkan.h>

namespace engine::rhi::vulkan {
DescriptorManager::DescriptorManager(
    VulkanContext&                       context,
    VulkanSwapchain&                     swapchain,
    const vk::raii::DescriptorSetLayout& descriptorSetLayout,
    const size_t                         uniformBufferSize)
: d_context(context)
, m_swapchain(swapchain)
, m_descriptorSetLayout(descriptorSetLayout)
, m_uniformBufferSize(uniformBufferSize)
{
    createUniformBuffers(uniformBufferSize);
    createDescriptorPool();
}

void DescriptorManager::createUniformBuffers(const size_t bufferSize)
{
    m_uniformBuffers.clear();
    m_uniformBuffersMemory.clear();
    m_uniformBuffersMapped.clear();

    const auto imageCount = static_cast<uint32_t>(
        m_swapchain.imageViews().size());

    for (size_t i = 0; i < imageCount; i++) {
        vk::BufferCreateInfo bufferInfo{};
        bufferInfo.size        = bufferSize;
        bufferInfo.usage       = vk::BufferUsageFlagBits::eUniformBuffer;
        bufferInfo.sharingMode = vk::SharingMode::eExclusive;

        vk::raii::Buffer buffer(d_context.device(), bufferInfo);

        const vk::MemoryRequirements memRequirements =
            buffer.getMemoryRequirements();
        const vk::PhysicalDeviceMemoryProperties memProperties =
            d_context.physicalDevice().getMemoryProperties();

        uint32_t memoryTypeIndex = UINT32_MAX;
        for (uint32_t j = 0; j < memProperties.memoryTypeCount; j++) {
            if (memRequirements.memoryTypeBits & 1 << j &&
                (memProperties.memoryTypes[j].propertyFlags &
                 (vk::MemoryPropertyFlagBits::eHostVisible |
                  vk::MemoryPropertyFlagBits::eHostCoherent)) ==
                    (vk::MemoryPropertyFlagBits::eHostVisible |
                     vk::MemoryPropertyFlagBits::eHostCoherent)) {
                memoryTypeIndex = j;
                break;
            }
        }

        if (memoryTypeIndex == UINT32_MAX) {
            throw std::runtime_error(
                "Failed to find suitable memory type for uniform buffer");
        }

        vk::MemoryAllocateInfo allocInfo{};
        allocInfo.allocationSize  = memRequirements.size;
        allocInfo.memoryTypeIndex = memoryTypeIndex;

        vk::raii::DeviceMemory bufferMemory(d_context.device(), allocInfo);
        buffer.bindMemory(*bufferMemory, 0);

        m_uniformBuffers.emplace_back(std::move(buffer));
        m_uniformBuffersMemory.emplace_back(std::move(bufferMemory));
        m_uniformBuffersMapped.emplace_back(
            m_uniformBuffersMemory[i].mapMemory(0, bufferSize));
    }
}

void DescriptorManager::createDescriptorPool()
{
    const auto imageCount = static_cast<uint32_t>(m_uniformBuffers.size());
    const uint32_t maxSets = 1000;
    std::array poolSize{
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, maxSets),
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler,
                               maxSets)};
    const vk::DescriptorPoolCreateInfo poolInfo(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        maxSets,
        poolSize);

    m_descriptorPool = vk::raii::DescriptorPool(d_context.device(), poolInfo);
}

const vk::raii::DescriptorSet& DescriptorManager::getDescriptorSet(
    const uint32_t             imageIndex,
    const vk::raii::ImageView& imageView,
    const vk::raii::Sampler&   sampler)
{
    DescriptorSetKey key{*imageView, *sampler};
    auto             it = m_descriptorSetsCache.find(key);

    if (it != m_descriptorSetsCache.end()) {
        return it->second[imageIndex];
    }

    // If not in cache, create sets for all swapchain images
    const std::vector layouts(m_uniformBuffers.size(), *m_descriptorSetLayout);
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool     = *m_descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(
        m_uniformBuffers.size());
    allocInfo.pSetLayouts = layouts.data();

    auto sets = d_context.device().allocateDescriptorSets(allocInfo);

    for (size_t i = 0; i < m_uniformBuffers.size(); i++) {
        vk::DescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = *m_uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range  = m_uniformBufferSize;

        vk::DescriptorImageInfo imageInfo{};
        imageInfo.sampler     = *sampler;
        imageInfo.imageView   = *imageView;
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        std::array<vk::WriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].dstSet          = *sets[i];
        descriptorWrites[0].dstBinding      = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].descriptorType =
            vk::DescriptorType::eUniformBuffer;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].dstSet          = *sets[i];
        descriptorWrites[1].dstBinding      = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].descriptorType =
            vk::DescriptorType::eCombinedImageSampler;
        descriptorWrites[1].pImageInfo = &imageInfo;

        d_context.device().updateDescriptorSets(descriptorWrites, {});
    }

    auto [newIt, inserted] =
        m_descriptorSetsCache.emplace(key, std::move(sets));
    return newIt->second[imageIndex];
}

void DescriptorManager::updateUniformBuffer(const uint32_t imageIndex,
                                            const void*    data) const
{
    memcpy(m_uniformBuffersMapped[imageIndex], data, m_uniformBufferSize);
}
}  // namespace core
