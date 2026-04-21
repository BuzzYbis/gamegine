// rhi_rendertargetprotocol.h -*-C++-*-
#ifndef INCLUDED_ENG_RHI_RENDERTARGETPROTOCOL_H
#define INCLUDED_ENG_RHI_RENDERTARGETPROTOCOL_H

//@PURPOSE: Provide a pure abstract interface for GPU render targets.
//
//@CLASSES:
//  eng::rhi::RenderTargetProtocol: Protocol for offscreen render targets.
//
//@DESCRIPTION: This component provides a pure abstract interface,
// 'eng::rhi::RenderTargetProtocol', that represents a destination for
// rendering operations other than the swapchain. It encapsulates an
// underlying texture that can be sampled by shaders after rendering.

// std
#include <cstdint>

namespace eng::rhi {

class TextureProtocol;

// ==========================
// class RenderTargetProtocol
// ==========================

/// This class provides a protocol (pure abstract interface) representing a
/// render target on the GPU.
class RenderTargetProtocol {
  public:
    // CREATORS

    /// Destroy this render target and release its associated resources.
    virtual ~RenderTargetProtocol() = default;

    // ACCESSORS

    /// Return the underlying texture associated with this render target.
    /// This texture can be used for sampling in shaders after the render
    /// target has been populated by a rendering pass.
    [[nodiscard]]
    virtual TextureProtocol* texture() const = 0;

    /// Return the width of this render target in pixels.
    [[nodiscard]]
    virtual uint32_t width() const = 0;

    /// Return the height of this render target in pixels.
    [[nodiscard]]
    virtual uint32_t height() const = 0;
};

}  // close package namespace

#endif  // INCLUDED_ENG_RHI_RENDERTARGETPROTOCOL_H
