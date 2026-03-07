// vk_recource_utils.cpp                                              -*-C++-*-
#include <rhi/vulkan/vk_buffer.h>
#include <rhi/vulkan/vk_context.h>
#include <rhi/vulkan/vk_resource_utils.h>

#include <stdexcept>

namespace engine::rhi::vulkan {
vk::raii::CommandBuffer
VulkanResourceUtils::beginSingleTimeCommands(const vk::raii::Device& device,
                                             const vk::CommandPool commandPool)
{
    vk::CommandBufferAllocateInfo allocInfo{};

    allocInfo.commandPool = commandPool;

    allocInfo.level              = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = 1;

    vk::raii::CommandBuffer commandBuffer = std::move(
        device.allocateCommandBuffers(allocInfo).front());

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

void VulkanResourceUtils::endSingleTimeCommands(
    const vk::raii::Queue&         queue,
    const vk::raii::CommandBuffer& commandBuffer)
{
    commandBuffer.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &*commandBuffer;
    queue.submit(submitInfo, nullptr);
    queue.waitIdle();
}

void VulkanResourceUtils::createImage(
    const engine::rhi::vulkan::VulkanContext& device,
    const uint32_t                            width,
    const uint32_t                            height,
    const uint32_t                            mipLevels,
    const vk::SampleCountFlagBits             numSamples,
    const vk::Format                          format,
    const vk::ImageTiling                     tiling,
    const vk::ImageUsageFlags                 usage,
    const vk::MemoryPropertyFlags             properties,
    vk::raii::Image&                          image,
    vk::raii::DeviceMemory&                   imageMemory)
{
    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType     = vk::ImageType::e2D;
    imageInfo.format        = format;
    imageInfo.extent.width  = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = mipLevels;
    imageInfo.arrayLayers   = 1;
    imageInfo.samples       = numSamples;
    imageInfo.tiling        = tiling;
    imageInfo.usage         = usage;
    imageInfo.sharingMode   = vk::SharingMode::eExclusive;

    image = vk::raii::Image(device.device(), imageInfo);

    const vk::MemoryRequirements memRequirements =
        image.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        engine::rhi::vulkan::VulkanBuffer::findMemoryTypeIndex(
            device,
            memRequirements.memoryTypeBits,
            properties);

    imageMemory = vk::raii::DeviceMemory(device.device(), allocInfo);
    image.bindMemory(*imageMemory, 0);
}

vk::raii::ImageView
VulkanResourceUtils::createImageView(const vk::raii::Device&    device,
                                     const vk::raii::Image&     image,
                                     const vk::Format           format,
                                     const vk::ImageAspectFlags aspectMask,
                                     uint32_t                   mipLevels)
{
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image            = *image;
    viewInfo.viewType         = vk::ImageViewType::e2D;
    viewInfo.format           = format;
    viewInfo.subresourceRange = {aspectMask, 0, mipLevels, 0, 1};
    return {device, viewInfo};
}

void VulkanResourceUtils::transitionImageLayout(
    const vk::raii::Device& device,
    const vk::CommandPool   commandPool,
    const vk::raii::Queue&  queue,
    const vk::raii::Image&  image,
    const vk::ImageLayout   oldLayout,
    const vk::ImageLayout   newLayout,
    uint32_t                mipLevels)
{
    const auto commandBuffer = beginSingleTimeCommands(device, commandPool);

    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout        = oldLayout;
    barrier.newLayout        = newLayout;
    barrier.image            = *image;
    barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor,
                                0,
                                mipLevels,
                                0,
                                1};

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if (oldLayout == vk::ImageLayout::eUndefined &&
        newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        sourceStage           = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage      = vk::PipelineStageFlagBits::eTransfer;
    }
    else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
             newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        sourceStage           = vk::PipelineStageFlagBits::eTransfer;
        destinationStage      = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    commandBuffer.pipelineBarrier(sourceStage,
                                  destinationStage,
                                  {},
                                  {},
                                  nullptr,
                                  barrier);

    endSingleTimeCommands(queue, commandBuffer);
}

void VulkanResourceUtils::copyBufferToImage(const vk::raii::Device& device,
                                            const vk::CommandPool  commandPool,
                                            const vk::raii::Queue& queue,
                                            const vk::raii::Buffer& buffer,
                                            const vk::raii::Image&  image,
                                            const uint32_t          width,
                                            const uint32_t          height)
{
    const auto commandBuffer = beginSingleTimeCommands(device, commandPool);

    vk::BufferImageCopy region{};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource  = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    region.imageOffset       = vk::Offset3D{0, 0, 0};
    region.imageExtent       = vk::Extent3D{width, height, 1};

    commandBuffer.copyBufferToImage(*buffer,
                                    *image,
                                    vk::ImageLayout::eTransferDstOptimal,
                                    {region});

    endSingleTimeCommands(queue, commandBuffer);
}

void VulkanResourceUtils::transition_image_layout(
    const vk::CommandBuffer&      commandBuffer,
    const vk::Image&              image,
    const vk::ImageLayout         old_layout,
    const vk::ImageLayout         new_layout,
    const vk::AccessFlags2        src_access_mask,
    const vk::AccessFlags2        dst_access_mask,
    const vk::PipelineStageFlags2 src_stage_mask,
    const vk::PipelineStageFlags2 dst_stage_mask,
    const vk::ImageAspectFlags    aspect_flags)
{
    vk::ImageMemoryBarrier2 barrier         = {};
    barrier.srcStageMask                    = src_stage_mask;
    barrier.srcAccessMask                   = src_access_mask;
    barrier.dstStageMask                    = dst_stage_mask;
    barrier.dstAccessMask                   = dst_access_mask;
    barrier.oldLayout                       = old_layout;
    barrier.newLayout                       = new_layout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = image;
    barrier.subresourceRange.aspectMask     = aspect_flags;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    vk::DependencyInfo dependency_info      = {};
    dependency_info.dependencyFlags         = {};
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers    = &barrier;
    commandBuffer.pipelineBarrier2(dependency_info);
}

}  // close engine::rhi::vulkan namespace
