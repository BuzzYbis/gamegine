#include <rhi/vulkan/vk_buffer.h>
#include <rhi/vulkan/vk_context.h>
#include <stdexcept>

namespace engine::rhi::vulkan {
uint32_t
VulkanBuffer::findMemoryTypeIndex(const VulkanContext&          device,
                                  const uint32_t                typeFilter,
                                  const vk::MemoryPropertyFlags properties)
{
    const vk::PhysicalDeviceMemoryProperties memProps =
        device.physicalDevice().getMemoryProperties();

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        const bool typeSupported = (typeFilter & (1u << i)) != 0;
        const bool flagsMatch    = (memProps.memoryTypes[i].propertyFlags &
                                 properties) == properties;
        if (typeSupported && flagsMatch) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

void VulkanBuffer::createBuffer(const VulkanContext&          device,
                                const vk::DeviceSize          size,
                                const vk::BufferUsageFlags    usage,
                                const vk::MemoryPropertyFlags properties,
                                vk::raii::Buffer&             buffer,
                                vk::raii::DeviceMemory&       memory)
{
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.size        = size;
    bufferInfo.usage       = usage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    buffer = vk::raii::Buffer(device.device(), bufferInfo);

    const vk::MemoryRequirements memReq = buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryTypeIndex(device,
                                                    memReq.memoryTypeBits,
                                                    properties);

    memory = vk::raii::DeviceMemory(device.device(), allocInfo);
    buffer.bindMemory(*memory, 0);
}

}  // close engine::rhi::vulkan namespace
