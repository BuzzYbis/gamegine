// rhi_textureprotocol.h                                              -*-C++-*-
#ifndef INCLUDED_ENG_RHI_TEXTUREPROTOCOL_H
#define INCLUDED_ENG_RHI_TEXTUREPROTOCOL_H

//@PURPOSE: Provide a pure abstract interface for GPU texture resources.
//
//@CLASSES:
//  eng::rhi::TextureProtocol: Protocol for GPU texture objects.
//
//@DESCRIPTION: This component provides a pure abstract interface,
// 'eng::rhi::TextureProtocol', that represents a texture resource allocated
// on the graphics card (GPU). It provides methods to access basic texture
// properties and handles the backend-specific details of image memory.

// std
#include <cstdint>

namespace eng::rhi {

// =====================
// class TextureProtocol
// =====================

/// This class provides a protocol (pure abstract interface) representing a
/// texture on the GPU.
class TextureProtocol {
  public:
    // CREATORS

    /// Destroy this texture and release its associated GPU memory.
    virtual ~TextureProtocol() = default;

    // ACCESSORS

    /// Return the width of this texture in pixels.
    [[nodiscard]]
    virtual uint32_t width() const = 0;

    /// Return the height of this texture in pixels.
    [[nodiscard]]
    virtual uint32_t height() const = 0;

    /// Return the number of mipmap levels available in this texture. For
    /// basic textures, this value is at least 1.
    [[nodiscard]]
    virtual uint32_t mipLevels() const = 0;
};

}  // close package namespace

#endif  // INCLUDED_ENG_RHI_TEXTUREPROTOCOL_H
