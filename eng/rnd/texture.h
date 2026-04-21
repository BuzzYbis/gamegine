// texture.h                                                         -*-C++-*-
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
#include <string>

// rhi
#include <rhi/rhi_contextprotocol.h>
#include <rhi/rhi_textureprotocol.h>

namespace eng::rnd {

// =============
// class Texture
// =============

/// This class provides a high-level representation of an image resource. It
/// handles the loading of pixel data from disk and manages the underlying
/// GPU memory via the RHI.
class Texture {
  private:
    // DATA

    /// The underlying RHI texture resource.
    std::unique_ptr<rhi::TextureProtocol> d_texture;

  public:
    // CREATORS

    /// Create a texture by loading and decoding the image at the specified
    /// 'texPath' using the specified RHI 'context'.
    Texture(rhi::ContextProtocol* context, const std::string& texPath);

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
