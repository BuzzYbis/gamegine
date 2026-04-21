// vlk_buffer.h                                                       -*-C++-*-
#ifndef INCLUDED_ENG_RHI_VLK_BUFFER_H
#define INCLUDED_ENG_RHI_VLK_BUFFER_H

//@PURPOSE: Provide a Vulkan-specific implementation of a GPU memory buffer.
//
//@CLASSES:
//  eng::rhi::vlk::Buffer: Vulkan implementation of BufferProtocol.
//
//@DESCRIPTION: This component provides a class, 'eng::rhi::vlk::Buffer',
// that implements the 'eng::rhi::BufferProtocol' for the Vulkan backend.
// It manages the allocation, mapping, and data transfer for a Vulkan buffer
// ('vk::Buffer') and its associated physical device memory
// ('vk::DeviceMemory').

// rhi
#include <rhi/rhi_bufferprotocol.h>

// vulkan
#include <vulkan/vulkan_raii.hpp>

namespace eng::rhi::vlk {

// Forward declaration
class Context;

// ============
// class Buffer
// ============

/// This class implements the RHI buffer protocol for the Vulkan backend.
/// It manages a block of contiguous GPU memory used to store vertices,
/// indices, or uniform data, and provides mechanisms to upload data from
/// the CPU to the GPU.
class Buffer : public BufferProtocol {
  private:
    // DATA

    /// Pointer to the Vulkan context used for allocation.
    Context* d_context_p;

    /// The total size of the buffer in bytes.
    size_t d_size;

    /// The intended usage of this buffer (e.g., Vertex, Index, Uniform).
    BufferUsage d_usage;

    /// Wrapped Vulkan buffer handle.
    vk::raii::Buffer d_buffer;

    /// Wrapped Vulkan device memory handle bound to the buffer.
    vk::raii::DeviceMemory d_deviceMemory;

  public:
    // CREATORS

    /// Create a Vulkan buffer of the specified 'size' in bytes configured
    /// for the specified 'usage', using the specified 'context' for resource
    /// allocation. The behavior is undefined unless 'context' remains valid
    /// for the lifetime of this object.
    explicit Buffer(Context* context, size_t size, BufferUsage usage);

    /// Destroy this buffer and release its associated Vulkan memory.
    ~Buffer() override = default;

    // MANIPULATORS

    /// Map the buffer's device memory to the host (CPU) address space and
    /// return a pointer to the beginning of the mapped memory. The behavior
    /// is undefined unless the buffer was created with host-visible memory.
    void* map() override;

    /// Unmap the buffer's device memory from the host address space.
    void unmap() override;

    /// Copy the specified 'size' in bytes from the specified 'data' pointer
    /// into the GPU buffer, starting at the specified 'offset'. The behavior
    /// is undefined unless 'data' is valid, and 'offset' + 'size' is less
    /// than or equal to the total size of this buffer.
    void uploadData(const void* data, size_t size, size_t offset) override;

    // ACCESSORS

    /// Return the total size of this buffer in bytes.
    [[nodiscard]]
    size_t size() const override;

    /// Return a const reference to the underlying Vulkan buffer handle.
    [[nodiscard]]
    const vk::raii::Buffer& buffer() const;
};

// =======================================================================
//                          INLINE DEFINITIONS
// =======================================================================

// ACCESSORS

inline size_t Buffer::size() const
{
    return d_size;
}

inline const vk::raii::Buffer& Buffer::buffer() const
{
    return d_buffer;
}

}  // close package namespace
#endif  // INCLUDED_ENG_RHI_VLK_BUFFER_H