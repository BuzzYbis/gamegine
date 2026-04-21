// vlk_rendertarget.h                                                 -*-C++-*-
#ifndef INCLUDED_ENG_RHI_VLK_RENDERTARGET_H
#define INCLUDED_ENG_RHI_VLK_RENDERTARGET_H

//@PURPOSE: Provide a Vulkan implementation of a GPU render target.
//
//@CLASSES:
//  eng::rhi::vlk::RenderTarget: Vulkan implementation of RenderTargetProtocol.
//
//@DESCRIPTION: This component provides a class, 'eng::rhi::vlk::RenderTarget',
// that implements the 'eng::rhi::RenderTargetProtocol' for the Vulkan
// backend. It manages an offscreen image that can be used as a color
// attachment during rendering and then sampled as a texture.

// rhi
#include <rhi/rhi_rendertargetprotocol.h>

// vlk
#include <rhi/vlk/vlk_context.h>
#include <rhi/vlk/vlk_texture.h>

// vulkan
#include <vulkan/vulkan_raii.hpp>

namespace eng::rhi::vlk {

// ==================
// class RenderTarget
// ==================

/// This class implements the RHI render target protocol for the Vulkan backend.
class RenderTarget : public RenderTargetProtocol {
  private:
    // DATA
    Context*              d_context_p;
    uint32_t              d_width;
    uint32_t              d_height;

    std::unique_ptr<Texture> d_texture;

    // Depth resources
    vk::raii::Image        d_depthImage;
    vk::raii::DeviceMemory d_depthMemory;
    vk::raii::ImageView    d_depthView;

  public:
    // CREATORS

    /// Create a Vulkan render target with the specified 'width', 'height',
    /// and 'format' using the specified 'context'.
    RenderTarget(Context* context, uint32_t width, uint32_t height, Format format);

    /// Destroy this render target.
    ~RenderTarget() override = default;

    // ACCESSORS

    TextureProtocol* texture() const override { return d_texture.get(); }
    uint32_t width() const override { return d_width; }
    uint32_t height() const override { return d_height; }

    /// Return the internal Vulkan texture.
    Texture* vlkTexture() const { return d_texture.get(); }

    /// Return the internal Vulkan depth view.
    const vk::raii::ImageView& depthView() const { return d_depthView; }

    /// Return the internal Vulkan depth image.
    const vk::raii::Image& depthImage() const { return d_depthImage; }
};

}  // close package namespace

#endif  // INCLUDED_ENG_RHI_VLK_RENDERTARGET_H
