// vlk_utils.cpp -*-C++-*-
#include <rhi/vlk/vlk_utils.h>

//@PURPOSE: Implement internal utilities for the Vulkan RHI.

// rhi
#include <rhi/rhi_types.h>

// std
#include <stdexcept>

namespace eng::rhi::vlk {

// ------------
// struct Utils
// ------------

// CLASS METHODS
uint32_t
Utils::findMemoryTypeIndex(const vk::raii::PhysicalDevice& physicalDevice,
                           const uint32_t                  typeFilter,
                           const vk::MemoryPropertyFlags   properties)
{
    const vk::PhysicalDeviceMemoryProperties memProps =
        physicalDevice.getMemoryProperties();

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) ==
                properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void Utils::createImage(Context*                      context,
                        const uint32_t                width,
                        const uint32_t                height,
                        const uint32_t                mipLevels,
                        const vk::SampleCountFlagBits numSamples,
                        const vk::Format              format,
                        const vk::ImageTiling         tiling,
                        const vk::ImageUsageFlags     usage,
                        const vk::MemoryPropertyFlags properties,
                        vk::raii::Image&              image,
                        vk::raii::DeviceMemory&       imageMemory)
{
    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType     = vk::ImageType::e2D;
    imageInfo.extent.width  = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = mipLevels;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = format;
    imageInfo.tiling        = tiling;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.usage         = usage;
    imageInfo.samples       = numSamples;
    imageInfo.sharingMode   = vk::SharingMode::eExclusive;

    image = vk::raii::Image(context->device(), imageInfo);

    const vk::MemoryRequirements memRequirements =
        image.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryTypeIndex(
        context->physicalDevice(),
        memRequirements.memoryTypeBits,
        properties);

    imageMemory = vk::raii::DeviceMemory(context->device(), allocInfo);
    image.bindMemory(*imageMemory, 0);
}

vk::raii::ImageView
Utils::createImageView(const vk::raii::Device&    device,
                       const vk::raii::Image&     image,
                       const vk::Format           format,
                       const vk::ImageAspectFlags aspectMask,
                       const uint32_t             mipLevels)
{
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image                           = *image;
    viewInfo.viewType                        = vk::ImageViewType::e2D;
    viewInfo.format                          = format;
    viewInfo.subresourceRange.aspectMask     = aspectMask;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    return {device, viewInfo};
}

void Utils::transitionImageLayout(const vk::raii::CommandBuffer& commandBuffer,
                                  const vk::raii::Image&         image,
                                  const vk::ImageLayout          oldLayout,
                                  const vk::ImageLayout          newLayout,
                                  const uint32_t                 mipLevels)
{
    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = *image;
    barrier.subresourceRange    = {vk::ImageAspectFlagBits::eColor,
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

        sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if (oldLayout == vk::ImageLayout::eUndefined &&
             newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
             newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage      = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    commandBuffer.pipelineBarrier(sourceStage,
                                  destinationStage,
                                  {},
                                  nullptr,
                                  nullptr,
                                  barrier);
}

vk::Format
Utils::findSupportedFormat(const vk::raii::PhysicalDevice& physicalDevice,
                           const std::vector<vk::Format>&  candidates,
                           const vk::ImageTiling           tiling,
                           const vk::FormatFeatureFlags    features)
{
    for (const auto format : candidates) {
        vk::FormatProperties props = physicalDevice.getFormatProperties(
            format);

        if (tiling == vk::ImageTiling::eLinear &&
            (props.linearTilingFeatures & features) == features) {
            return format;
        }
        if (tiling == vk::ImageTiling::eOptimal &&
            (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}

vk::Format
Utils::findDepthFormat(const vk::raii::PhysicalDevice& physicalDevice)
{
    return findSupportedFormat(
        physicalDevice,
        {vk::Format::eD32Sfloat,
         vk::Format::eD32SfloatS8Uint,
         vk::Format::eD24UnormS8Uint},
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

vk::Format Utils::getVkFormat(const Format format)
{
    switch (format) {
    case Format::R8G8B8A8_UNorm: return vk::Format::eR8G8B8A8Unorm;
    case Format::R8G8B8A8_SRGB: return vk::Format::eR8G8B8A8Srgb;
    case Format::B8G8R8A8_SRGB: return vk::Format::eB8G8R8A8Srgb;
    case Format::D32_SFloat: return vk::Format::eD32Sfloat;
    default: return vk::Format::eUndefined;
    }
}

}  // close package namespace
