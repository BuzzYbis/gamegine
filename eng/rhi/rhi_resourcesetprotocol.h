// rhi_resourcesetprotocol.h                                          -*-C++-*-
#ifndef INCLUDED_ENG_RHI_RESOURCESETPROTOCOL_H
#define INCLUDED_ENG_RHI_RESOURCESETPROTOCOL_H

//@PURPOSE: Provide a pure abstract interface for GPU resource sets.
//
//@CLASSES:
//  eng::rhi::ResourceSetProtocol: Protocol for binding GPU resources.
//
//@DESCRIPTION: This component provides a pure abstract interface,
// 'eng::rhi::ResourceSetProtocol', that represents a set of resources
// (buffers, textures) bound to specific slots in a graphics pipeline. It
// allows for the efficient update of shader data and textures from the CPU.

// std
#include <cstddef>
#include <cstdint>

namespace eng::rhi {

// Forward declarations
class BufferProtocol;
class TextureProtocol;

// =========================
// class ResourceSetProtocol
// =========================

/// This class provides a protocol (pure abstract interface) representing a
/// set of GPU resources that can be bound to a pipeline.
class ResourceSetProtocol {
  public:
    // CREATORS

    /// Destroy this resource set and release its internal handles.
    virtual ~ResourceSetProtocol() = default;

    // MANIPULATORS

    /// Bind the specified 'buffer' to the specified 'binding' slot in this
    /// set, with the specified 'offset' and 'range'.
    virtual void updateBuffer(uint32_t        binding,
                              BufferProtocol* buffer,
                              size_t          offset,
                              size_t          range) = 0;

    /// Bind the specified 'texture' to the specified 'binding' slot in this
    /// set.
    virtual void updateTexture(uint32_t binding, TextureProtocol* texture) = 0;
};

}  // close package namespace
#endif  // INCLUDED_ENG_RHI_RESOURCESETPROTOCOL_H