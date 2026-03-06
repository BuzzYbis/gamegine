// material.h                                                       -*-C++-*-
#ifndef INCLUDED_ENGINE_RENDERER_MATERIAL
#define INCLUDED_ENGINE_RENDERER_MATERIAL

// renderer
#include <renderer/texture.h>

// rhi
#include <rhi/vulkan/vk_pipeline.h>

namespace engine::renderer {

class Material {
  private:
    rhi::vulkan::VulkanPipeline* d_pipeline_p    = nullptr;
    Texture*                     d_texture       = nullptr;
    vk::DescriptorSet            d_descriptorSet = nullptr;

  public:
    Material()  = default;
    ~Material() = default;

    [[nodiscard]] const vk::raii::Pipeline& pipeline() const
    {
        return d_pipeline_p->pipeline();
    }

    void setPipeline(rhi::vulkan::VulkanPipeline* pipeline)
    {
        d_pipeline_p = pipeline;
    }

    [[nodiscard]] const vk::raii::PipelineLayout& layout() const
    {
        return d_pipeline_p->layout();
    }

    void setTexture(Texture* texture) { d_texture = texture; }

    [[nodiscard]] Texture* texture() const { return d_texture; }

    void setDescriptorSet(const vk::DescriptorSet set)
    {
        d_descriptorSet = set;
    }
    [[nodiscard]] vk::DescriptorSet descriptorSet() const;
};

inline vk::DescriptorSet Material::descriptorSet() const
{
    return d_descriptorSet;
}

}  // namespace engine::renderer

#endif  // INCLUDED_ENGINE_RENDERER_MATERIAL
