// vlk_resourcelayout.h                                               -*-C++-*-
#ifndef INCLUDED_ENG_RHI_VLK_RESOURCELAYOUT_H
#define INCLUDED_ENG_RHI_VLK_RESOURCELAYOUT_H

//@PURPOSE: Provide a Vulkan-specific implementation of a resource layout.
//
//@CLASSES:
//  eng::rhi::vlk::ResourceLayout: Vulkan backend for 'ResourceLayoutProtocol'.
//
//@DESCRIPTION: This component provides a class, 'eng::rhi::vlk::ResourceLayout',
// that implements the 'eng::rhi::ResourceLayoutProtocol' for the Vulkan
// backend. It encapsulates a 'vk::DescriptorSetLayout', which defines the
// binding points and resource types for descriptor sets.

// rhi
#include <rhi/rhi_resourcelayoutprotocol.h>

// vulkan
#include <vulkan/vulkan_raii.hpp>

namespace eng::rhi::vlk {

// Forward declaration
class Context;

// ====================
// class ResourceLayout
// ====================

/// This class implements the resource layout protocol for the Vulkan backend.
class ResourceLayout : public ResourceLayoutProtocol {
  private:
    // DATA

    /// Pointer to the Vulkan context (held, not owned).
    Context* d_context_p;

    /// The underlying Vulkan descriptor set layout handle.
    vk::raii::DescriptorSetLayout d_layout;

  public:
    // CREATORS

    /// Create a Vulkan resource layout using the specified 'context' and
    /// 'config'. The behavior is undefined unless 'context' remains valid
    /// for the lifetime of this object.
    explicit ResourceLayout(Context*                    context,
                            const ResourceLayoutConfig& config);

    /// Destroy this resource layout and release the Vulkan handle.
    ~ResourceLayout() override = default;

    // ACCESSORS

    /// Return a reference to the underlying Vulkan descriptor set layout.
    [[nodiscard]]
    const vk::raii::DescriptorSetLayout& layout() const;
};

inline const vk::raii::DescriptorSetLayout& ResourceLayout::layout() const
{
    return d_layout;
}

}  // close package namespace
#endif  // INCLUDED_ENG_RHI_VLK_RESOURCELAYOUT_H