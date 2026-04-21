// rhi_bufferprotocol.h                                               -*-C++-*-
#ifndef INCLUDED_ENG_RHI_BUFFERPROTOCOL_H
#define INCLUDED_ENG_RHI_BUFFERPROTOCOL_H

//@PURPOSE: Provide a pure abstract interface for GPU memory buffers.
//
//@CLASSES:
//  eng::rhi::BufferProtocol: Protocol for GPU buffer resources.
//
//@DESCRIPTION: This component provides a pure abstract interface,
// 'eng::rhi::BufferProtocol', that represents a contiguous block of memory
// allocated on the graphics card (GPU). Buffers are used to store various
// types of data, such as vertices, indices, or uniform shader variables.

// std
#include <cstddef>

namespace eng::rhi {

/// Enumerates the intended usage of a GPU buffer resource.
enum class BufferUsage {
    Vertex,   // Buffer contains vertex data (positions, normals, UVs).
    Index,    // Buffer contains indices for drawing elements.
    Uniform,  // Buffer contains uniform variables (e.g., matrices) for
              // shaders.
    Staging   // Buffer is a temporary CPU-visible buffer used for transfers.
};

// ====================
// class BufferProtocol
// ====================

/// This class provides a protocol (pure abstract interface) representing a
/// memory buffer on the GPU. It provides methods to map the memory to the CPU
/// for direct writing, or to upload data directly to the device.
class BufferProtocol {
  public:
    // CREATORS

    /// Destroy this buffer and release its associated GPU memory.
    virtual ~BufferProtocol() = default;

    // MANIPULATORS

    /// Map the buffer's device memory to the host (CPU) address space and
    /// return a pointer to the beginning of the mapped memory. The behavior
    /// is undefined unless the buffer was created with host-visible memory.
    virtual void* map() = 0;

    /// Unmap the buffer's device memory from the host address space.
    virtual void unmap() = 0;

    /// Copy the specified 'size' in bytes from the specified 'data' pointer
    /// into the GPU buffer, starting at the specified 'offset'. The behavior
    /// is undefined unless 'data' is valid, and 'offset' + 'size' is less
    /// than or equal to the total size of this buffer.
    virtual void uploadData(const void* data, size_t size, size_t offset) = 0;

    // ACCESSORS

    /// Return the total size of this buffer in bytes.
    [[nodiscard]]
    virtual size_t size() const = 0;
};

}  // close package namespace
#endif  // INCLUDED_ENG_RHI_BUFFERPROTOCOL_H