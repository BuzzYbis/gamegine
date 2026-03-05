// VulkanTexture.hpp                                                 -*-C++-*-
#ifndef INCLUDED_CORE_VULKANTEXTURE
#define INCLUDED_CORE_VULKANTEXTURE

//@PURPOSE: Provide an RAII wrapper for a sampled 2D texture in Vulkan.
//
//@CLASSES:
//  core::VulkanTexture: Loads an image file and creates image/view/sampler.
//
//@DESCRIPTION: This component encapsulates the Vulkan objects required to use
// a texture in shaders: 'vk::Image', its memory, a 'vk::ImageView', and a
// 'vk::Sampler'. It uses staging buffers to upload pixels to the GPU.

#include <renderer/command_pool.h>
#include <string>
#include <vulkan/vulkan_raii.hpp>

namespace engine::renderer {

class Texture {
  public:
    Texture(rhi::vulkan::VulkanContext& context,
            vk::CommandPool             commandPool,
            const std::string&          texPath,
            vk::Format                  format = vk::Format::eR8G8B8A8Srgb);

    const vk::raii::ImageView& getImageView() const { return m_imageView; }
    const vk::raii::Sampler&   getSampler() const { return m_sampler; }

  private:
    rhi::vulkan::VulkanContext& d_context;

    vk::Format             d_format;
    vk::raii::Image        m_image       = nullptr;
    vk::raii::DeviceMemory m_imageMemory = nullptr;
    vk::raii::ImageView    m_imageView   = nullptr;
    vk::raii::Sampler      m_sampler     = nullptr;
    /// Number of mip levels in the image (LOD)
    uint32_t m_mipLevels = 0;

    void generateMipmaps(const vk::raii::Image& image,
                         vk::Format             imageFormat,
                         int32_t                texWidth,
                         int32_t                texHeight,
                         uint32_t               mipLevels,
                         vk::CommandPool        commandPool) const;
};
}  // namespace core

#endif  // INCLUDED_CORE_VULKANTEXTURE
