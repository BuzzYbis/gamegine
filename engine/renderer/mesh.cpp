#include "renderer/mesh.h"
#include "rhi/vulkan/vk_buffer.h"

namespace engine::renderer {
namespace {
void copyBuffer(const rhi::vulkan::VulkanContext& context,
                vk::CommandPool                   commandPool,
                const vk::raii::Buffer&           srcBuffer,
                const vk::raii::Buffer&           dstBuffer,
                const vk::DeviceSize              size)
{
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool             = commandPool;
    allocInfo.level                   = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount      = 1;
    const vk::raii::CommandBuffer cmd = std::move(
        context.device().allocateCommandBuffers(allocInfo).front());

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmd.begin(beginInfo);
    cmd.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy(0, 0, size));
    cmd.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &*cmd;

    context.graphicsQueue().submit(submitInfo, nullptr);
    context.graphicsQueue().waitIdle();
}
}  // close unamed namespace

Mesh::Mesh(rhi::vulkan::VulkanContext&      context,
           vk::CommandPool                  commandPool,
           const std::vector<core::Vertex>& vertices,
           const std::vector<uint32_t>&     indices)
: d_context(context)
, m_indexCount(static_cast<uint32_t>(indices.size()))
{
    // Vertex buffer
    const vk::DeviceSize vertexBufferSize = sizeof(vertices[0]) *
                                            vertices.size();

    vk::raii::Buffer       vertexStagingBuffer(nullptr);
    vk::raii::DeviceMemory vertexStagingMemory(nullptr);
    rhi::vulkan::VulkanBuffer::createBuffer(
        d_context,
        vertexBufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent,
        vertexStagingBuffer,
        vertexStagingMemory);

    void* vData = vertexStagingMemory.mapMemory(0, vertexBufferSize);
    std::memcpy(vData, vertices.data(), vertexBufferSize);
    vertexStagingMemory.unmapMemory();

    rhi::vulkan::VulkanBuffer::createBuffer(
        d_context,
        vertexBufferSize,
        vk::BufferUsageFlagBits::eTransferDst |
            vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_vertexBuffer,
        m_vertexBufferMemory);

    copyBuffer(d_context,
               commandPool,
               vertexStagingBuffer,
               m_vertexBuffer,
               vertexBufferSize);

    // Index buffer
    const vk::DeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();

    vk::raii::Buffer       indexStagingBuffer(nullptr);
    vk::raii::DeviceMemory indexStagingMemory(nullptr);
    rhi::vulkan::VulkanBuffer::createBuffer(
        d_context,
        indexBufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent,
        indexStagingBuffer,
        indexStagingMemory);

    void* iData = indexStagingMemory.mapMemory(0, indexBufferSize);
    std::memcpy(iData, indices.data(), indexBufferSize);
    indexStagingMemory.unmapMemory();

    rhi::vulkan::VulkanBuffer::createBuffer(
        d_context,
        indexBufferSize,
        vk::BufferUsageFlagBits::eTransferDst |
            vk::BufferUsageFlagBits::eIndexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_indexBuffer,
        m_indexBufferMemory);

    copyBuffer(d_context,
               commandPool,
               indexStagingBuffer,
               m_indexBuffer,
               indexBufferSize);
}

void Mesh::draw(const vk::CommandBuffer cmd) const
{
    const vk::Buffer vb              = *m_vertexBuffer;
    constexpr vk::DeviceSize offsets = 0;
    cmd.bindVertexBuffers(0, 1, &vb, &offsets);
    cmd.bindIndexBuffer(*m_indexBuffer, 0, vk::IndexType::eUint32);
    cmd.drawIndexed(m_indexCount, 1, 0, 0, 0);
}
}  // namespace core
