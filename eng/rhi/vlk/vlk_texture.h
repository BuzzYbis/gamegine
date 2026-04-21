// vlk_texture.h                                                      -*-C++-*-
#ifndef INCLUDED_ENG_RHI_VLK_TEXTURE_H
#define INCLUDED_ENG_RHI_VLK_TEXTURE_H

//@PURPOSE: Provide a Vulkan implementation of a GPU texture resource.
//
//@CLASSES:
//  eng::rhi::vlk::Texture: Vulkan implementation of TextureProtocol.
//
//@DESCRIPTION: This component provides a class, 'eng::rhi::vlk::Texture',
// that implements the 'eng::rhi::TextureProtocol' for the Vulkan backend.
// It manages the lifecycle of a Vulkan image, its memory allocation,
// its image view, and its sampler.

// rhi
#include <rhi/rhi_textureprotocol.h>

// vlk
#include <rhi/vlk/vlk_context.h>

// vulkan
#include <vulkan/vulkan_raii.hpp>

namespace eng::rhi::vlk {

// =============
// class Texture
// =============

/// This class implements the RHI texture protocol for the Vulkan backend.
class Texture : public TextureProtocol {
  private:
    // DATA
    Context*               d_context_p;
    uint32_t               d_width;
    uint32_t               d_height;
    uint32_t               d_mipLevels;

    vk::raii::Image        d_image;
    vk::raii::DeviceMemory d_memory;
    vk::raii::ImageView    d_view;
    vk::raii::Sampler      d_sampler;

    // PRIVATE MANIPULATORS

    /// Generate mipmaps for the texture's image.
    void generateMipmaps(const vk::raii::Image& image,
                         int32_t                texWidth,
                         int32_t                texHeight,
                         uint32_t               mipLevels);

  public:
    // CREATORS

    /// Create a Vulkan texture with the specified dimensions, mip levels,
    /// format, and optional 'pixels' data. If 'pixels' is null, the texture
    /// is created with RenderTarget usage flags.
    Texture(Context*      context,
            uint32_t      width,
            uint32_t      height,
            uint32_t      mipLevels,
            vk::Format    format,
            const void*   pixels = nullptr);

    /// Destroy this texture and release associated Vulkan resources.
    ~Texture() override = default;

    // ACCESSORS

    uint32_t width() const override { return d_width; }
    uint32_t height() const override { return d_height; }
    uint32_t mipLevels() const override { return d_mipLevels; }

    /// Return the Vulkan image view.
    const vk::raii::ImageView& view() const { return d_view; }

    /// Return the Vulkan sampler.
    const vk::raii::Sampler& sampler() const { return d_sampler; }

    /// Return the Vulkan image.
    const vk::raii::Image& vlkImage() const { return d_image; }
};

}  // close package namespace

#endif  // INCLUDED_ENG_RHI_VLK_TEXTURE_H
