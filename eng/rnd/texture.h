// texture.h                                                          -*-C++-*-
#ifndef INCLUDED_ENG_RND_TEXTURE_H
#define INCLUDED_ENG_RND_TEXTURE_H

//@PURPOSE: Provide a high-level texture object for the renderer.
//
//@CLASSES:
//  eng::rnd::Texture: High-level texture resource.
//
//@DESCRIPTION: This component provides a high-level 'Texture' class used
// by the renderer to manage GPU texture resources. It wraps an
// 'rhi::TextureProtocol' implementation and provides a clean interface
// for the rest of the engine.

// std
#include <memory>
#include <span>
#include <string>

// rhi
#include <rhi/rhi_contextprotocol.h>
#include <rhi/rhi_textureprotocol.h>
#include <rhi/rhi_types.h>

namespace eng::rnd {

// =============
// class Texture
// =============

/// This class provides a high-level representation of an image resource. It
/// handles the loading of pixel data from disk and manages the underlying
/// GPU memory via the RHI.
///
/// Every constructor takes the format its pixels are to be given on the
/// device. That format is a property of the role the image plays rather than
/// of the image itself: the same file read as a base color is sRGB encoded,
/// and read as a normal or a roughness map holds linear values that the
/// hardware must not convert.
class Texture {
  private:
    // DATA

    /// The underlying RHI texture resource.
    std::unique_ptr<rhi::TextureProtocol> d_texture;

    // PRIVATE MANIPULATORS

    /// Upload to the GPU, using the specified RHI 'context', the specified
    /// 'pixels' of the specified 'width' and 'height', in RGBA8 layout and
    /// the specified 'format', and generate their mip chain.
    void upload(rhi::ContextProtocol* context,
                const unsigned char*  pixels,
                int                   width,
                int                   height,
                rhi::Format           format);

  public:
    // CREATORS

    /// Create a texture by loading and decoding the image at the specified
    /// 'texPath' using the specified RHI 'context', and give its pixels the
    /// specified 'format' on the device.
    Texture(rhi::ContextProtocol* context,
            const std::string&    texPath,
            rhi::Format           format = rhi::Format::R8G8B8A8_SRGB);

    /// Create a texture by decoding the specified 'encoded' image bytes,
    /// holding a PNG or a JPEG, using the specified RHI 'context', and give
    /// its pixels the specified 'format' on the device. The specified
    /// 'label' identifies the image in diagnostic messages. Throw
    /// 'std::runtime_error' if 'encoded' does not hold a supported image.
    Texture(rhi::ContextProtocol*          context,
            std::span<const unsigned char> encoded,
            const std::string&             label,
            rhi::Format format = rhi::Format::R8G8B8A8_SRGB);

    /// Create a texture holding the specified 'pixels', laid out as RGBA8
    /// rows of the specified 'width' and 'height', uploaded with the
    /// specified 'format' using the specified RHI 'context'. Throw
    /// 'std::runtime_error' unless 'pixels' holds exactly
    /// 'width * height * 4' bytes. This constructor builds the small solid
    /// images a material binds to the slots it declares no texture for.
    Texture(rhi::ContextProtocol*          context,
            std::span<const unsigned char> pixels,
            int                            width,
            int                            height,
            rhi::Format                    format);

    /// Destroy this texture and release its GPU resources.
    ~Texture() = default;

    // ACCESSORS

    /// Return a pointer to the underlying RHI texture protocol implementation.
    [[nodiscard]]
    rhi::TextureProtocol* protocol() const
    {
        return d_texture.get();
    }
};

}  // close package namespace

#endif  // INCLUDED_ENG_RND_TEXTURE_H
