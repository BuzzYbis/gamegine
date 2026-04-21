// vlk_rendertarget.cpp                                               -*-C++-*-
#include <rhi/vlk/vlk_rendertarget.h>

//@PURPOSE: Implement the Vulkan RHI render target.

// rhi
#include <rhi/rhi_types.h>

// vlk
#include <rhi/vlk/vlk_utils.h>

namespace eng::rhi::vlk {

// ------------------
// class RenderTarget
// ------------------

// CREATORS
RenderTarget::RenderTarget(Context*           context,
                           const uint32_t     width,
                           const uint32_t     height,
                           const rhi::Format  format)
: d_context_p(context)
, d_width(width)
, d_height(height)
, d_depthImage(nullptr)
, d_depthMemory(nullptr)
, d_depthView(nullptr)
{
    // 1. Create Color Texture
    // A render target is essentially a texture that can be used as a color
    // attachment during a rendering pass. We create it with the necessary
    // usage flags for both rendering and sampling.
    d_texture = std::make_unique<Texture>(d_context_p,
                                          width,
                                          height,
                                          1, 
                                          Utils::getVkFormat(format),
                                          nullptr);

    // 2. Create Depth Resources
    // To support depth testing during offscreen rendering, we allocate a
    // dedicated depth image (Z-buffer) and its corresponding view.
    const vk::Format depthFormat = Utils::findDepthFormat(d_context_p->physicalDevice());
    
    Utils::createImage(d_context_p,
                       width,
                       height,
                       1,
                       vk::SampleCountFlagBits::e1,
                       depthFormat,
                       vk::ImageTiling::eOptimal,
                       vk::ImageUsageFlagBits::eDepthStencilAttachment,
                       vk::MemoryPropertyFlagBits::eDeviceLocal,
                       d_depthImage,
                       d_depthMemory);

    d_depthView = Utils::createImageView(d_context_p->device(),
                                         d_depthImage,
                                         depthFormat,
                                         vk::ImageAspectFlagBits::eDepth,
                                         1);
}

}  // close package namespace
