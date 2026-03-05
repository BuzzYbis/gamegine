// material.cpp                                                      -*-C++-*-

#include <renderer/material.h>

#include <rhi/vulkan/vk_pipeline.h>

namespace engine::renderer {

Material::Material(rhi::vulkan::VulkanContext& context,
                   vk::Format                  colorFormat,
                   vk::Format                  depthFormat,
                   vk::DescriptorSetLayout     setLayout)
{
    d_pipeline_p = std::make_unique<rhi::vulkan::VulkanPipeline>(context,
                                                                 colorFormat,
                                                                 depthFormat,
                                                                 setLayout);
}

}  // namespace engine::renderer
