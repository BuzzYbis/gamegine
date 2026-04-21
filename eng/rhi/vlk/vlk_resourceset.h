// vlk_resourceset.h                                                  -*-C++-*-
#ifndef INCLUDED_ENG_RHI_VLK_RESOURCESET_H
#define INCLUDED_ENG_RHI_VLK_RESOURCESET_H

//@PURPOSE: Provide a Vulkan-specific implementation of a resource set.
//
//@CLASSES:
//  eng::rhi::vlk::ResourceSet: Vulkan backend for 'ResourceSetProtocol'.
//
//@DESCRIPTION: This component provides a class, 'eng::rhi::vlk::ResourceSet',
// that implements the 'eng::rhi::ResourceSetProtocol' for the Vulkan
// backend. It encapsulates a 'vk::DescriptorSet', which holds the actual
// GPU addresses of resources (buffers, textures) bound to a pipeline.

// rhi
#include <rhi/rhi_resourcesetprotocol.h>

// vulkan
#include <vulkan/vulkan_raii.hpp>

namespace eng::rhi::vlk {

// Forward declarations
class Context;
class ResourceLayout;

// =================
// class ResourceSet
// =================

/// This class implements the resource set protocol for the Vulkan backend.
class ResourceSet : public ResourceSetProtocol {
  private:
    // DATA

    /// Pointer to the Vulkan context (held, not owned).
    Context* d_context_p;

    /// The underlying Vulkan descriptor set handle.
    vk::raii::DescriptorSet d_set;

  public:
    // CREATORS

    /// Create a Vulkan resource set using the specified 'context' and bound
    /// to the specified 'layout'. The behavior is undefined unless
    /// 'context' and 'layout' are valid.
    explicit ResourceSet(Context* context, const ResourceLayout* layout);

    /// Destroy this resource set and release the descriptor handle.
    ~ResourceSet() override = default;

    // MANIPULATORS

    /// Bind the specified 'buffer' to the specified 'binding' slot in this
    /// set, with the specified 'offset' and 'range'.
    void updateBuffer(uint32_t        binding,
                      BufferProtocol* buffer,
                      size_t          offset,
                      size_t          range) override;

    /// Bind the specified 'texture' to the specified 'binding' slot in this
    /// set.
    void updateTexture(uint32_t binding, TextureProtocol* texture) override;

    // ACCESSORS

    /// Return a reference to the underlying Vulkan descriptor set handle.
    [[nodiscard]]
    const vk::raii::DescriptorSet& set() const;
};

inline const vk::raii::DescriptorSet& ResourceSet::set() const
{
    return d_set;
}

}  // close package namespace
#endif  // INCLUDED_ENG_RHI_VLK_RESOURCESET_H