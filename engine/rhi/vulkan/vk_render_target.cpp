// vk_render_target.cpp                                               -*-C++-*-
#include <rhi/vulkan/vk_render_target.h>
#include <rhi/vulkan/vk_resource_utils.h>
#include <stdexcept>

namespace engine::rhi::vulkan {

VulkanRenderTarget::VulkanRenderTarget(VulkanContext&  context,
                                       vk::Extent2D    extent,
                                       vk::Format      format,
                                       vk::CommandPool commandPool)
: d_context(context)
, d_extent(extent)
, d_format(format)
{
    vk::Extent3D            extent3D    = {d_extent.width, d_extent.height, 1};
    vk::SampleCountFlagBits msaaSamples = d_context.msaaSamples();

    // 1. MSAA Image (Si activé)
    if (msaaSamples != vk::SampleCountFlagBits::e1) {
        vk::ImageCreateInfo msaaInfo{};
        msaaInfo.imageType   = vk::ImageType::e2D;
        msaaInfo.format      = d_format;
        msaaInfo.extent      = extent3D;
        msaaInfo.mipLevels   = 1;
        msaaInfo.arrayLayers = 1;
        msaaInfo.samples     = msaaSamples;
        msaaInfo.tiling      = vk::ImageTiling::eOptimal;
        msaaInfo.usage       = vk::ImageUsageFlagBits::eColorAttachment;

        d_msaaImage  = vk::raii::Image(d_context.device(), msaaInfo);
        d_msaaMemory = allocateImageMemory(d_msaaImage);
        d_msaaImage.bindMemory(*d_msaaMemory, 0);

        vk::ImageViewCreateInfo msaaViewInfo{};
        msaaViewInfo.image    = *d_msaaImage;
        msaaViewInfo.viewType = vk::ImageViewType::e2D;
        msaaViewInfo.format   = d_format;
        msaaViewInfo.subresourceRange.aspectMask =
            vk::ImageAspectFlagBits::eColor;
        msaaViewInfo.subresourceRange.baseMipLevel   = 0;
        msaaViewInfo.subresourceRange.levelCount     = 1;
        msaaViewInfo.subresourceRange.baseArrayLayer = 0;
        msaaViewInfo.subresourceRange.layerCount     = 1;

        d_msaaView = vk::raii::ImageView(d_context.device(), msaaViewInfo);
    }

    // 2. Resolve Image (Cible finale)
    vk::ImageCreateInfo resolveInfo{};
    resolveInfo.imageType   = vk::ImageType::e2D;
    resolveInfo.format      = d_format;
    resolveInfo.extent      = extent3D;
    resolveInfo.mipLevels   = 1;
    resolveInfo.arrayLayers = 1;
    resolveInfo.samples     = vk::SampleCountFlagBits::e1;
    resolveInfo.tiling      = vk::ImageTiling::eOptimal;
    resolveInfo.usage       = vk::ImageUsageFlagBits::eColorAttachment |
                        vk::ImageUsageFlagBits::eSampled;

    d_image  = vk::raii::Image(d_context.device(), resolveInfo);
    d_memory = allocateImageMemory(d_image);
    d_image.bindMemory(*d_memory, 0);

    vk::ImageViewCreateInfo resolveViewInfo{};
    resolveViewInfo.image    = *d_image;
    resolveViewInfo.viewType = vk::ImageViewType::e2D;
    resolveViewInfo.format   = d_format;
    resolveViewInfo.subresourceRange.aspectMask =
        vk::ImageAspectFlagBits::eColor;
    resolveViewInfo.subresourceRange.baseMipLevel   = 0;
    resolveViewInfo.subresourceRange.levelCount     = 1;
    resolveViewInfo.subresourceRange.baseArrayLayer = 0;
    resolveViewInfo.subresourceRange.layerCount     = 1;

    d_view = vk::raii::ImageView(d_context.device(), resolveViewInfo);

    // 3. Sampler
    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter    = vk::Filter::eLinear;
    samplerInfo.minFilter    = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;

    d_sampler = vk::raii::Sampler(d_context.device(), samplerInfo);

    // 4. Transitions de layout
    vk::raii::CommandBuffer cmd = VulkanResourceUtils::beginSingleTimeCommands(
        d_context.device(),
        commandPool);

    VulkanResourceUtils::transition_image_layout(
        cmd,
        *d_image,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::AccessFlagBits2::eNone,
        vk::AccessFlagBits2::eShaderRead,
        vk::PipelineStageFlagBits2::eTopOfPipe,
        vk::PipelineStageFlagBits2::eFragmentShader,
        vk::ImageAspectFlagBits::eColor);

    if (msaaSamples != vk::SampleCountFlagBits::e1) {
        VulkanResourceUtils::transition_image_layout(
            cmd,
            *d_msaaImage,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::AccessFlagBits2::eNone,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::ImageAspectFlagBits::eColor);
    }

    VulkanResourceUtils::endSingleTimeCommands(d_context.graphicsQueue(), cmd);
}

vk::raii::DeviceMemory
VulkanRenderTarget::allocateImageMemory(const vk::raii::Image& image) const
{
    const vk::MemoryRequirements memRequirements =
        image.getMemoryRequirements();
    const vk::PhysicalDeviceMemoryProperties memProperties =
        d_context.physicalDevice().getMemoryProperties();
    uint32_t memoryTypeIndex = 0;
    bool     found           = false;

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (memRequirements.memoryTypeBits & 1 << i &&
            (memProperties.memoryTypes[i].propertyFlags &
             vk::MemoryPropertyFlagBits::eDeviceLocal) ==
                vk::MemoryPropertyFlagBits::eDeviceLocal) {
            memoryTypeIndex = i;
            found           = true;
            break;
        }
    }

    if (!found) {
        throw std::runtime_error("Error : Unable to find a memory type "
                                 "for the render target");
    }

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    return {d_context.device(), allocInfo};
}

}  // namespace engine::rhi::vulkan