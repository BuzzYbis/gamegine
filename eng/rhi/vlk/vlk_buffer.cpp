// vlk_buffer.cpp                                                     -*-C++-*-
#include <rhi/vlk/vlk_buffer.h>

// std
#include <stdexcept>

// rhi
#include <rhi/vlk/vlk_context.h>

namespace eng::rhi::vlk {
namespace {

uint32_t findMemoryTypeIndex(const vk::raii::PhysicalDevice& physicalDevice,
                             const uint32_t                  typeFilter,
                             const vk::MemoryPropertyFlags   properties)
{
    const vk::PhysicalDeviceMemoryProperties memProps =
        physicalDevice.getMemoryProperties();

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        const bool typeSupported = (typeFilter & 1u << i) != 0;
        const bool flagsMatch    = (memProps.memoryTypes[i].propertyFlags &
                                 properties) == properties;
        if (typeSupported && flagsMatch) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type.");
}

vk::BufferUsageFlags getVkBufferUsage(const BufferUsage usage)
{
    switch (usage) {
    case BufferUsage::Vertex: return vk::BufferUsageFlagBits::eVertexBuffer;
    case BufferUsage::Index: return vk::BufferUsageFlagBits::eIndexBuffer;
    case BufferUsage::Uniform: return vk::BufferUsageFlagBits::eUniformBuffer;
    case BufferUsage::Staging: return vk::BufferUsageFlagBits::eTransferSrc;
    default: return {};
    }
}

}  // close anonymous namespace

Buffer::Buffer(Context* context, const size_t size, const BufferUsage usage)
: d_context_p(context)
, d_size(size)
, d_usage(usage)
, d_buffer(nullptr)
, d_deviceMemory(nullptr)
{
    // Logical Buffer Creation
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.size        = d_size;
    bufferInfo.usage       = getVkBufferUsage(d_usage);
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    d_buffer = vk::raii::Buffer(d_context_p->device(), bufferInfo);

    // Physical Memory Allocation
    const vk::MemoryRequirements memReq = d_buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize = memReq.size;

    // Note: Currently forcing all buffers to be HostVisible/HostCoherent.
    allocInfo.memoryTypeIndex = findMemoryTypeIndex(
        d_context_p->physicalDevice(),
        memReq.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);

    d_deviceMemory = vk::raii::DeviceMemory(d_context_p->device(), allocInfo);

    // Memory Binding
    d_buffer.bindMemory(*d_deviceMemory, 0);
}

void* Buffer::map()
{
    return d_deviceMemory.mapMemory(0, d_size);
}

void Buffer::unmap()
{
    d_deviceMemory.unmapMemory();
}

void Buffer::uploadData(const void*  data,
                        const size_t size,
                        const size_t offset)
{
    if (size + offset > d_size) {
        throw std::out_of_range("Upload size exceeds buffer capacity.");
    }

    void* mappedMemory = map();
    char* destination  = static_cast<char*>(mappedMemory) + offset;
    std::memcpy(destination, data, size);
    unmap();
}

}  // close package namespace