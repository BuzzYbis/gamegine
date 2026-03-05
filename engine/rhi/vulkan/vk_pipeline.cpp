// vk_pipeline.cpp                                                    -*-C++-*-
#include <rhi/vulkan/vk_pipeline.h>

// core
#include <core/log.h>
#include <core/vertex.h>

// rhi
#include <rhi/vulkan/vk_types.h>

// std
#include <fstream>
#include <iostream>

namespace engine::rhi::vulkan {

namespace {

std::vector<char> readFile(const std::string& filename)
{
    LOG_INFO("Read file: ", filename);
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }
    std::vector<char> buffer(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
    return buffer;
}

vk::raii::ShaderModule createShaderModule(const vk::raii::Device&  device,
                                          const std::vector<char>& code)
{
    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.codeSize = code.size() * sizeof(char);
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    vk::raii::ShaderModule shaderModule{device, createInfo};

    return shaderModule;
}

}  // close unnamed namespace

VulkanPipeline::VulkanPipeline(VulkanContext&          context,
                               vk::Format              swapchainFormat,
                               vk::Format              depthFormat,
                               vk::DescriptorSetLayout descriptorSetLayout)
: d_context(context)
{
    // Read shader code
    auto shaderCode = readFile("shaders/shader.spv");

    // Create shader module from shader code
    vk::raii::ShaderModule shaderModule =
        createShaderModule(d_context.device(), shaderCode);

    // Create vertex shader stage, allow to transform 3D world to 2D screen
    // space
    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.stage  = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = shaderModule;
    vertShaderStageInfo.pName  = "vertMain";

    // Create fragment shader stage, determine the color and opacity of faces
    // of the model
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.stage  = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = shaderModule;
    fragShaderStageInfo.pName  = "fragMain";

    // Create shader stages array, allow to give both vertex and fragment
    // shaders to the pipeline
    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                        fragShaderStageInfo};

    // Describes the overall data chunk for a single vertex.
    auto bindingDescription = core::Vertex::getBindingDescription();

    // Describe the individual properties inside that chunk.
    auto attributeDescriptions = core::Vertex::getAttributeDescriptions();

    // The vertex input state tells the GPU how to interpret the raw bytes of
    // memory coming from our vertex buffers and map them to the input
    // structure of our vertex shader (our `VSInput` struct in Slang).
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(
        attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions =
        attributeDescriptions.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable        = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.polygonMode             = vk::PolygonMode::eFill;
    rasterizer.cullMode                = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace               = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable         = vk::False;
    rasterizer.depthBiasSlopeFactor    = 1.0f;
    rasterizer.lineWidth               = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.rasterizationSamples = d_context.msaaSamples();
    multisampling.sampleShadingEnable  = vk::False;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable    = vk::False;
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR |
                                          vk::ColorComponentFlagBits::eG |
                                          vk::ColorComponentFlagBits::eB |
                                          vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.logicOpEnable   = vk::False;
    colorBlending.logicOp         = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    std::vector dynamicStates = {vk::DynamicState::eViewport,
                                 vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(
        dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Depth stencil state (required if depthFormat is provided)
    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    if (depthFormat != vk::Format::eUndefined) {
        depthStencil.depthTestEnable       = vk::True;
        depthStencil.depthWriteEnable      = vk::True;
        depthStencil.depthCompareOp        = vk::CompareOp::eLess;
        depthStencil.depthBoundsTestEnable = vk::False;
        depthStencil.stencilTestEnable     = vk::False;
    }

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts    = &descriptorSetLayout;

    vk::PushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(MeshPushConstants);
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;

    d_layout = vk::raii::PipelineLayout(d_context.device(),
                                        pipelineLayoutInfo);

    vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
    graphicsPipelineCreateInfo.stageCount          = 2;
    graphicsPipelineCreateInfo.pStages             = shaderStages;
    graphicsPipelineCreateInfo.pVertexInputState   = &vertexInputInfo;
    graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    graphicsPipelineCreateInfo.pViewportState      = &viewportState;
    graphicsPipelineCreateInfo.pRasterizationState = &rasterizer;
    graphicsPipelineCreateInfo.pMultisampleState   = &multisampling;
    graphicsPipelineCreateInfo.pColorBlendState    = &colorBlending;
    graphicsPipelineCreateInfo.pDynamicState       = &dynamicState;
    graphicsPipelineCreateInfo.pDepthStencilState =
        depthFormat != vk::Format::eUndefined ? &depthStencil : nullptr;
    graphicsPipelineCreateInfo.layout     = *d_layout;
    graphicsPipelineCreateInfo.renderPass = nullptr;

    vk::PipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.colorAttachmentCount    = 1;
    renderingInfo.pColorAttachmentFormats = &swapchainFormat;
    renderingInfo.depthAttachmentFormat   = depthFormat;

    vk::StructureChain pipelineCreateInfoChain{graphicsPipelineCreateInfo,
                                               renderingInfo};

    d_pipeline = vk::raii::Pipeline(
        d_context.device(),
        nullptr,
        pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}

}  // close engine::rhi::vulkan
