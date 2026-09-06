// vlk_resourcelayout.cpp                                             -*-C++-*-
#include <rhi/rhi_types.h>

// rhi
#include <rhi/vlk/vlk_context.h>
#include <rhi/vlk/vlk_resourcelayout.h>

namespace eng::rhi::vlk {
namespace {

vk::DescriptorType getVkDescriptorType(ResourceType type)
{
    switch (type) {
    case ResourceType::UniformBuffer:
        return vk::DescriptorType::eUniformBuffer;
    case ResourceType::TextureSampler:
        return vk::DescriptorType::eCombinedImageSampler;
    default: return vk::DescriptorType::eUniformBuffer;
    }
}

vk::ShaderStageFlags getVkShaderStageFlags(ShaderStage stage)
{
    switch (stage) {
    case ShaderStage::Vertex: return vk::ShaderStageFlagBits::eVertex;
    case ShaderStage::Fragment: return vk::ShaderStageFlagBits::eFragment;
    case ShaderStage::VertexFragment:
        return vk::ShaderStageFlagBits::eVertex |
               vk::ShaderStageFlagBits::eFragment;
    default: return vk::ShaderStageFlagBits::eAllGraphics;
    }
}

}  // unnamed namespace

ResourceLayout::ResourceLayout(Context*                    context,
                               const ResourceLayoutConfig& config)
: d_context_p(context)
, d_layout(nullptr)
{
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    bindings.reserve(config.bindings.size());

    for (const auto& [binding, type, stage] : config.bindings) {
        bindings.emplace_back(
            binding,
            getVkDescriptorType(type),
            1,  // descriptorCount (1 unless it's a texture array)
            getVkShaderStageFlags(stage),
            nullptr);
    }

    vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings);
    d_layout = vk::raii::DescriptorSetLayout(d_context_p->device(),
                                             layoutInfo);
}

}  // close package namespace