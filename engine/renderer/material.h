// material.h                                                       -*-C++-*-
#ifndef INCLUDED_ENGINE_RENDERER_MATERIAL
#define INCLUDED_ENGINE_RENDERER_MATERIAL

#include "texture.h"

#include <memory>
#include <rhi/vulkan/vk_pipeline.h>

namespace engine::renderer {

class Material {
  private:
    std::unique_ptr<rhi::vulkan::VulkanPipeline> d_pipeline_p;
    Texture*                                     d_texture       = nullptr;
    vk::DescriptorSet                            d_descriptorSet = nullptr;

  public:
    Material(rhi::vulkan::VulkanContext& context,
             vk::Format                  colorFormat,
             vk::Format                  depthFormat,
             vk::DescriptorSetLayout     setLayout);
    ~Material() = default;

    const vk::raii::Pipeline& pipeline() const
    {
        return d_pipeline_p->pipeline();
    }

    const vk::raii::PipelineLayout& layout() const
    {
        if (!d_pipeline_p || !*d_pipeline_p->layout()) {
            throw std::runtime_error("pipeline is null");
        }
        return d_pipeline_p->layout();
    }

    void setTexture(Texture* texture) { d_texture = texture; }

    Texture* texture() const { return d_texture; }

    void setDescriptorSet(const vk::DescriptorSet set)
    {
        d_descriptorSet = set;
    }
    vk::DescriptorSet descriptorSet() const;
};

inline vk::DescriptorSet Material::descriptorSet() const
{
    return d_descriptorSet;
}

}  // namespace engine::renderer
#endif  // INCLUDED_ENGINE_RENDERER_MATERIAL
