// vlk_resourceset.cpp                                                -*-C++-*-
#include <rhi/vlk/vlk_resourceset.h>

// rhi
#include <rhi/vlk/vlk_buffer.h>
#include <rhi/vlk/vlk_context.h>
#include <rhi/vlk/vlk_resourcelayout.h>
#include <rhi/vlk/vlk_texture.h>

namespace eng::rhi::vlk {

ResourceSet::ResourceSet(Context* context, const ResourceLayout* layout)
: d_context_p(context)
, d_set(nullptr)
{
    // Allocate the set from the context pool using the provided layout.
    const vk::DescriptorSetLayout       vkLayout = *layout->layout();
    const vk::DescriptorSetAllocateInfo allocInfo(
        *d_context_p->descriptorPool(),
        1,
        &vkLayout);

    // vulkan.hpp returns a std::vector; take the first element.
    d_set = std::move(
        vk::raii::DescriptorSets(d_context_p->device(), allocInfo).front());
}

void ResourceSet::updateBuffer(const uint32_t  binding,
                               BufferProtocol* buffer,
                               const size_t    offset,
                               const size_t    range)
{
    // 1. Unmask the Vulkan buffer.
    const auto* vlkBuffer = static_cast<Buffer*>(buffer);

    // 2. Prepare descriptor info (if range is 0, use the entire buffer).
    const vk::DescriptorBufferInfo bufferInfo(*vlkBuffer->buffer(),
                                              offset,
                                              range == 0 ? VK_WHOLE_SIZE
                                                         : range);

    // 3. Request the GPU to update the binding.
    const vk::WriteDescriptorSet write(*d_set,
                                       binding,
                                       0,
                                       1,
                                       vk::DescriptorType::eUniformBuffer,
                                       nullptr,
                                       &bufferInfo,
                                       nullptr);

    d_context_p->device().updateDescriptorSets(write, nullptr);
}

void ResourceSet::updateTexture(const uint32_t   binding,
                                TextureProtocol* texture)
{
    const auto* vlkTexture = static_cast<Texture*>(texture);

    // Bind the sampler and the view of the texture
    const vk::DescriptorImageInfo imageInfo(
        *vlkTexture->sampler(),
        *vlkTexture->view(),
        vk::ImageLayout::eShaderReadOnlyOptimal);

    const vk::WriteDescriptorSet write(
        *d_set,
        binding,
        0,
        1,
        vk::DescriptorType::eCombinedImageSampler,
        &imageInfo,
        nullptr,
        nullptr);

    d_context_p->device().updateDescriptorSets(write, nullptr);
}

}  // close package namespace