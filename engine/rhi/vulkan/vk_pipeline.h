// vk_pipeline.h                                                      -*-C++-*-
#ifndef INCLUDED_ENGINE_RHI_VULKAN_VK_PIPELINE_H
#define INCLUDED_ENGINE_RHI_VULKAN_VK_PIPELINE_H

// rhi
#include <rhi/vulkan/vk_context.h>

// vulkan
#include <vulkan/vulkan_raii.hpp>

namespace engine::rhi::vulkan {

struct PipelineConfig {
    vk::PolygonMode   polygonMode      = vk::PolygonMode::eFill;
    vk::CullModeFlags cullMode         = vk::CullModeFlagBits::eBack;
    bool              enableDepthTest  = true;
    bool              enableDepthWrite = true;
    bool              enableBlending   = false;
};

// --------------------
// class VulkanPipeline
// --------------------
class VulkanPipeline {
  private:
    // DATA
    VulkanContext&           d_context;
    vk::raii::PipelineLayout d_layout   = nullptr;
    vk::raii::Pipeline       d_pipeline = nullptr;

  public:
    // CREATORS

    VulkanPipeline(VulkanContext&          context,
                   vk::Format              swapchainFormat,
                   vk::Format              depthFormat,
                   vk::DescriptorSetLayout descriptorSetLayout,
                   const PipelineConfig&   config = PipelineConfig());

    // ACCESSORS

    [[nodiscard]] const vk::raii::Pipeline& pipeline() const;

    [[nodiscard]] const vk::raii::PipelineLayout& layout() const;
};

// =======================================================================
//                          INLINE DEFINITIONS
// =======================================================================

// ACCESSORS

inline const vk::raii::Pipeline& VulkanPipeline::pipeline() const
{
    return d_pipeline;
}

inline const vk::raii::PipelineLayout& VulkanPipeline::layout() const
{
    return d_layout;
}

}  // close engine::rhi::vulkan namespace

#endif  // INCLUDED_ENGINE_RHI_VULKAN_VK_PIPELINE_H