// vlk_pipeline.h                                                     -*-C++-*-
#ifndef INCLUDED_ENG_RHI_VLK_PIPELINE_H
#define INCLUDED_ENG_RHI_VLK_PIPELINE_H

//@PURPOSE: Provide a Vulkan-specific implementation of a graphics pipeline.
//
//@CLASSES:
//  eng::rhi::vlk::Pipeline: Vulkan backend for 'PipelineProtocol'.
//
//@DESCRIPTION: This component provides a concrete, Vulkan-specific
// implementation of the 'eng::rhi::PipelineProtocol'. The
// 'eng::rhi::vlk::Pipeline' class is responsible for translating the
// platform-agnostic 'PipelineConfig' into Vulkan-specific structures and
// baking them into a 'vk::Pipeline' state object. It also manages the
// 'vk::PipelineLayout' which defines the interface (push constants and
// descriptor sets) between the shaders and the CPU.

// vulkan
#include <vulkan/vulkan_raii.hpp>

// rhi
#include <rhi/rhi_pipelineprotocol.h>

namespace eng::rhi::vlk {

class Context;

// ==============
// class Pipeline
// ==============

/// This class implements the graphics pipeline protocol for the Vulkan
/// backend. It encapsulates the raw Vulkan pipeline and its layout, managing
/// their lifetimes via RAII.
class Pipeline : public PipelineProtocol {
  private:
    // DATA

    /// Pointer to the Vulkan context (held, not owned).
    Context* d_context_p;

    /// The interface layout for the shaders.
    vk::raii::PipelineLayout d_layout;

    /// The baked graphics pipeline state.
    vk::raii::Pipeline d_pipeline;

  public:
    // CREATORS

    /// Create a Vulkan graphics pipeline using the specified 'context' and
    /// 'config'. The 'config' defines all the fixed states (shaders, blending,
    /// rasterization) required to bake the pipeline. The behavior is undefined
    /// unless 'context' is non-null and points to a valid Vulkan context.
    explicit Pipeline(Context* context, const PipelineConfig& config);

    /// Destroy this pipeline object and release the underlying Vulkan
    /// resources.
    ~Pipeline() override = default;

    // ACCESSORS

    /// Return a reference to the underlying Vulkan pipeline object.
    [[nodiscard]]
    const vk::raii::Pipeline& pipeline() const;

    /// Return a reference to the underlying Vulkan pipeline layout object.
    [[nodiscard]]
    const vk::raii::PipelineLayout& layout() const;
};

// ============================================================================
//                             INLINE DEFINITIONS
// ============================================================================

inline const vk::raii::Pipeline& Pipeline::pipeline() const
{
    return d_pipeline;
}

inline const vk::raii::PipelineLayout& Pipeline::layout() const
{
    return d_layout;
}

}  // close package namespace
#endif  // INCLUDED_ENG_RHI_VLK_PIPELINE_H