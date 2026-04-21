// vlk_texture.cpp                                                     -*-C++-*-
#include <rhi/vlk/vlk_texture.h>

// rhi
#include <rhi/rhi_types.h>

// vlk
#include <rhi/vlk/vlk_utils.h>

// std
#include <algorithm>
#include <stdexcept>

namespace eng::rhi::vlk {

// -------------
// class Texture
// -------------

// CREATORS
Texture::Texture(Context*      context,
                 const uint32_t width,
                 const uint32_t height,
                 const uint32_t mipLevels,
                 const vk::Format format,
                 const void*   pixels)
: d_context_p(context)
, d_width(width)
, d_height(height)
, d_mipLevels(mipLevels)
, d_image(nullptr)
, d_memory(nullptr)
, d_view(nullptr)
, d_sampler(nullptr)
{
    vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled;
    
    if (pixels) {
        usage |= vk::ImageUsageFlagBits::eTransferSrc | 
                 vk::ImageUsageFlagBits::eTransferDst;
                 
        const vk::DeviceSize imageSize = static_cast<vk::DeviceSize>(width) *
                                         static_cast<vk::DeviceSize>(height) * 4;

        // 1. Create Staging Buffer
        vk::BufferCreateInfo stagingInfo{};
        stagingInfo.size        = imageSize;
        stagingInfo.usage       = vk::BufferUsageFlagBits::eTransferSrc;
        stagingInfo.sharingMode = vk::SharingMode::eExclusive;

        vk::raii::Buffer stagingBuffer(d_context_p->device(), stagingInfo);

        const vk::MemoryRequirements memReq = stagingBuffer.getMemoryRequirements();
        vk::MemoryAllocateInfo       allocInfo{};
        allocInfo.allocationSize  = memReq.size;
        allocInfo.memoryTypeIndex = Utils::findMemoryTypeIndex(
            d_context_p->physicalDevice(),
            memReq.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eHostVisible |
                vk::MemoryPropertyFlagBits::eHostCoherent);

        vk::raii::DeviceMemory stagingMemory(d_context_p->device(), allocInfo);
        stagingBuffer.bindMemory(*stagingMemory, 0);

        // 2. Upload Pixels
        void* data = stagingMemory.mapMemory(0, imageSize);
        std::memcpy(data, pixels, static_cast<size_t>(imageSize));
        stagingMemory.unmapMemory();

        // 3. Create GPU Image
        Utils::createImage(d_context_p,
                           width,
                           height,
                           mipLevels,
                           vk::SampleCountFlagBits::e1,
                           format,
                           vk::ImageTiling::eOptimal,
                           usage,
                           vk::MemoryPropertyFlagBits::eDeviceLocal,
                           d_image,
                           d_memory);

        // 4. Copy Buffer to Image
        vk::CommandPoolCreateInfo poolInfo{};
        poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;
        poolInfo.queueFamilyIndex = d_context_p->graphicsQueueFamilyIndex();
        vk::raii::CommandPool pool(d_context_p->device(), poolInfo);

        vk::CommandBufferAllocateInfo cmdAlloc{};
        cmdAlloc.commandPool        = *pool;
        cmdAlloc.level              = vk::CommandBufferLevel::ePrimary;
        cmdAlloc.commandBufferCount = 1;

        vk::raii::CommandBuffer cmd = std::move(
            vk::raii::CommandBuffers(d_context_p->device(), cmdAlloc).front());

        cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

        Utils::transitionImageLayout(cmd,
                                     d_image,
                                     vk::ImageLayout::eUndefined,
                                     vk::ImageLayout::eTransferDstOptimal,
                                     mipLevels);

        vk::BufferImageCopy region{};
        region.bufferOffset      = 0;
        region.bufferRowLength   = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource  = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
        region.imageOffset       = vk::Offset3D(0, 0, 0);
        region.imageExtent       = vk::Extent3D(width, height, 1);

        cmd.copyBufferToImage(*stagingBuffer,
                              *d_image,
                              vk::ImageLayout::eTransferDstOptimal,
                              region);

        cmd.end();

        vk::SubmitInfo submitInfo{};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &*cmd;
        d_context_p->graphicsQueue().submit(submitInfo, nullptr);
        d_context_p->graphicsQueue().waitIdle();

        // 5. Mipmaps
        generateMipmaps(d_image, width, height, mipLevels);
    }
    else {
        // Render Target Case
        usage |= vk::ImageUsageFlagBits::eColorAttachment;
        
        Utils::createImage(d_context_p,
                           width,
                           height,
                           mipLevels,
                           vk::SampleCountFlagBits::e1,
                           format,
                           vk::ImageTiling::eOptimal,
                           usage,
                           vk::MemoryPropertyFlagBits::eDeviceLocal,
                           d_image,
                           d_memory);

        // Transition to a state ready for sampling/rendering
        vk::CommandPoolCreateInfo poolInfo{};
        poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;
        poolInfo.queueFamilyIndex = d_context_p->graphicsQueueFamilyIndex();
        vk::raii::CommandPool pool(d_context_p->device(), poolInfo);

        vk::CommandBufferAllocateInfo cmdAlloc{};
        cmdAlloc.commandPool        = *pool;
        cmdAlloc.level              = vk::CommandBufferLevel::ePrimary;
        cmdAlloc.commandBufferCount = 1;

        vk::raii::CommandBuffer cmd = std::move(
            vk::raii::CommandBuffers(d_context_p->device(), cmdAlloc).front());

        cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

        Utils::transitionImageLayout(cmd,
                                     d_image,
                                     vk::ImageLayout::eUndefined,
                                     vk::ImageLayout::eShaderReadOnlyOptimal,
                                     mipLevels);

        cmd.end();

        vk::SubmitInfo submitInfo{};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &*cmd;
        d_context_p->graphicsQueue().submit(submitInfo, nullptr);
        d_context_p->graphicsQueue().waitIdle();
    }

    // 6. View & Sampler
    d_view = Utils::createImageView(d_context_p->device(),
                                    d_image,
                                    format,
                                    vk::ImageAspectFlagBits::eColor,
                                    mipLevels);

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter    = vk::Filter::eLinear;
    samplerInfo.minFilter    = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
    samplerInfo.anisotropyEnable =
        d_context_p->physicalDevice().getFeatures().samplerAnisotropy;
    samplerInfo.maxAnisotropy =
        d_context_p->physicalDevice().getProperties().limits.maxSamplerAnisotropy;
    samplerInfo.borderColor             = vk::BorderColor::eIntOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = vk::False;
    samplerInfo.compareEnable           = vk::False;
    samplerInfo.compareOp               = vk::CompareOp::eAlways;
    samplerInfo.mipmapMode              = vk::SamplerMipmapMode::eLinear;
    samplerInfo.minLod                  = 0.0f;
    samplerInfo.maxLod                  = static_cast<float>(mipLevels);
    samplerInfo.mipLodBias              = 0.0f;

    d_sampler = vk::raii::Sampler(d_context_p->device(), samplerInfo);
}

void Texture::generateMipmaps(const vk::raii::Image& image,
                              const int32_t          texWidth,
                              const int32_t          texHeight,
                              const uint32_t         mipLevels)
{
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;
    poolInfo.queueFamilyIndex = d_context_p->graphicsQueueFamilyIndex();
    vk::raii::CommandPool pool(d_context_p->device(), poolInfo);

    vk::CommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.commandPool        = *pool;
    cmdAlloc.level              = vk::CommandBufferLevel::ePrimary;
    cmdAlloc.commandBufferCount = 1;

    vk::raii::CommandBuffer cmd = std::move(
        vk::raii::CommandBuffers(d_context_p->device(), cmdAlloc).front());

    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    vk::ImageMemoryBarrier barrier{};
    barrier.image                           = *image;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
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

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                            vk::PipelineStageFlagBits::eTransfer,
                            {},
                            nullptr,
                            nullptr,
                            barrier);

        vk::ImageBlit blit{};
        blit.srcOffsets[0] = vk::Offset3D(0, 0, 0);
        blit.srcOffsets[1] = vk::Offset3D(mipWidth, mipHeight, 1);
        blit.srcSubresource.aspectMask     = vk::ImageAspectFlagBits::eColor;
        blit.srcSubresource.mipLevel       = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount     = 1;
        blit.dstOffsets[0]                 = vk::Offset3D(0, 0, 0);
        blit.dstOffsets[1]                 = vk::Offset3D(mipWidth > 1 ? mipWidth / 2 : 1,
                                            mipHeight > 1 ? mipHeight / 2 : 1,
                                            1);
        blit.dstSubresource.aspectMask     = vk::ImageAspectFlagBits::eColor;
        blit.dstSubresource.mipLevel       = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount     = 1;

        cmd.blitImage(*image,
                      vk::ImageLayout::eTransferSrcOptimal,
                      *image,
                      vk::ImageLayout::eTransferDstOptimal,
                      blit,
                      vk::Filter::eLinear);

        barrier.oldLayout     = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                            vk::PipelineStageFlagBits::eFragmentShader,
                            {},
                            nullptr,
                            nullptr,
                            barrier);

        if (mipWidth > 1)
            mipWidth /= 2;
        if (mipHeight > 1)
            mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout     = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eFragmentShader,
                        {},
                        nullptr,
                        nullptr,
                        barrier);

    cmd.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &*cmd;
    d_context_p->graphicsQueue().submit(submitInfo, nullptr);
    d_context_p->graphicsQueue().waitIdle();
}

}  // close package namespace
