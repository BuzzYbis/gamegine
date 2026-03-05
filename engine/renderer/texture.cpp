#include <renderer/command_pool.h>
#include <renderer/texture.h>
#include <rhi/vulkan/vk_buffer.h>
#include <rhi/vulkan/vk_resource_utils.h>

#include <iostream>
#include <stb_image.h>
#include <stdexcept>

namespace engine::renderer {
Texture::Texture(rhi::vulkan::VulkanContext& context,
                 vk::CommandPool             commandPool,
                 const std::string&          texPath,
                 const vk::Format            format)
: d_context(context)
, d_format(format)
{
    int      texWidth    = 0;
    int      texHeight   = 0;
    int      texChannels = 0;
    stbi_uc* pixels      = stbi_load(texPath.c_str(),
                                &texWidth,
                                &texHeight,
                                &texChannels,
                                STBI_rgb_alpha);

    if (!pixels) {
        throw std::runtime_error("failed to load texture image: " + texPath);
    }

    const vk::DeviceSize imageSize = static_cast<vk::DeviceSize>(texWidth) *
                                     static_cast<vk::DeviceSize>(texHeight) *
                                     4;

    // Calculates the number of levels in the mip chain
    // 1. Get the largest dimension between the textWidth and textHeight (max)
    // 2. Check how many times that dimension can be divided by 2 (log2)
    // 3. Handle the case where the dimension is not a power of 2 (floor)
    // 4. Add 1 to include the original image has a mip level
    m_mipLevels = static_cast<uint32_t>(
                      std::floor(std::log2(std::max(texWidth, texHeight)))) +
                  1;

    vk::raii::Buffer       stagingBuffer({});
    vk::raii::DeviceMemory stagingBufferMemory({});
    rhi::vulkan::VulkanBuffer::createBuffer(
        d_context,
        imageSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer,
        stagingBufferMemory);

    void* data = stagingBufferMemory.mapMemory(0, imageSize);
    std::memcpy(data, pixels, imageSize);
    stagingBufferMemory.unmapMemory();
    stbi_image_free(pixels);

    // Reserves memory and sets physical properties
    rhi::vulkan::VulkanResourceUtils::createImage(
        d_context,
        static_cast<uint32_t>(texWidth),
        static_cast<uint32_t>(texHeight),
        m_mipLevels,
        vk::SampleCountFlagBits::e1,
        d_format,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eTransferDst |
            vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_image,
        m_imageMemory);

    rhi::vulkan::VulkanResourceUtils::transitionImageLayout(
        d_context.device(),
        commandPool,
        d_context.graphicsQueue(),
        m_image,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        m_mipLevels);

    rhi::vulkan::VulkanResourceUtils::copyBufferToImage(
        d_context.device(),
        commandPool,
        d_context.graphicsQueue(),
        stagingBuffer,
        m_image,
        static_cast<uint32_t>(texWidth),
        static_cast<uint32_t>(texHeight));

    generateMipmaps(m_image,
                    d_format,
                    texWidth,
                    texHeight,
                    m_mipLevels,
                    commandPool);

    m_imageView = rhi::vulkan::VulkanResourceUtils::createImageView(
        d_context.device(),
        m_image,
        d_format,
        vk::ImageAspectFlagBits::eColor,
        m_mipLevels);

    // Sampler
    const vk::PhysicalDeviceProperties properties =
        d_context.physicalDevice().getProperties();
    const vk::PhysicalDeviceFeatures features =
        d_context.physicalDevice().getFeatures();

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter        = vk::Filter::eLinear;
    samplerInfo.minFilter        = vk::Filter::eLinear;
    samplerInfo.mipmapMode       = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU     = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV     = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW     = vk::SamplerAddressMode::eRepeat;
    samplerInfo.mipLodBias       = 0.0f;
    samplerInfo.anisotropyEnable = features.samplerAnisotropy ? VK_TRUE
                                                              : VK_FALSE;
    samplerInfo.maxAnisotropy    = features.samplerAnisotropy
                                       ? properties.limits.maxSamplerAnisotropy
                                       : 1.0f;
    samplerInfo.compareEnable    = VK_FALSE;
    samplerInfo.compareOp        = vk::CompareOp::eAlways;
    samplerInfo.minLod           = 0.0f;
    samplerInfo.maxLod           = vk::LodClampNone;

    m_sampler = vk::raii::Sampler(d_context.device(), samplerInfo);
}
void Texture::generateMipmaps(const vk::raii::Image& image,
                              const vk::Format       imageFormat,
                              const int32_t          texWidth,
                              const int32_t          texHeight,
                              const uint32_t         mipLevels,
                              const vk::CommandPool  commandPool) const
{
    // Check if image format supports linear blit-ing
    const vk::FormatProperties formatProperties =
        d_context.physicalDevice().getFormatProperties(imageFormat);

    if (!(formatProperties.optimalTilingFeatures &
          vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
        throw std::runtime_error(
            "texture image format does not support linear blitting!");
    }

    const auto commandBuffer =
        rhi::vulkan::VulkanResourceUtils::beginSingleTimeCommands(
            d_context.device(),
            commandPool);

    vk::ImageMemoryBarrier barrier(vk::AccessFlagBits::eTransferWrite,
                                   vk::AccessFlagBits::eTransferRead,
                                   vk::ImageLayout::eTransferDstOptimal,
                                   vk::ImageLayout::eTransferSrcOptimal,
                                   vk::QueueFamilyIgnored,
                                   vk::QueueFamilyIgnored,
                                   image);
    barrier.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.subresourceRange.levelCount     = 1;

    int32_t mipWidth  = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout     = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout     = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                      vk::PipelineStageFlagBits::eTransfer,
                                      {},
                                      {},
                                      {},
                                      barrier);

        vk::ArrayWrapper1D<vk::Offset3D, 2> offsets, dstOffsets;
        offsets[0]    = vk::Offset3D(0, 0, 0);
        offsets[1]    = vk::Offset3D(mipWidth, mipHeight, 1);
        dstOffsets[0] = vk::Offset3D(0, 0, 0);
        dstOffsets[1] = vk::Offset3D(mipWidth > 1 ? mipWidth / 2 : 1,
                                     mipHeight > 1 ? mipHeight / 2 : 1,
                                     1);
        vk::ImageBlit blit  = {};
        blit.srcOffsets     = offsets;
        blit.dstOffsets     = dstOffsets;
        blit.srcSubresource = vk::ImageSubresourceLayers(
            vk::ImageAspectFlagBits::eColor,
            i - 1,
            0,
            1);
        blit.dstSubresource = vk::ImageSubresourceLayers(
            vk::ImageAspectFlagBits::eColor,
            i,
            0,
            1);

        commandBuffer.blitImage(image,
                                vk::ImageLayout::eTransferSrcOptimal,
                                image,
                                vk::ImageLayout::eTransferDstOptimal,
                                {blit},
                                vk::Filter::eLinear);

        barrier.oldLayout     = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader,
            {},
            {},
            {},
            barrier);

        if (mipWidth > 1) {
            mipWidth /= 2;
        }
        if (mipHeight > 1) {
            mipHeight /= 2;
        }
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout     = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                  vk::PipelineStageFlagBits::eFragmentShader,
                                  {},
                                  {},
                                  {},
                                  barrier);

    rhi::vulkan::VulkanResourceUtils::endSingleTimeCommands(
        d_context.graphicsQueue(),
        commandBuffer);
}
}  // namespace core
