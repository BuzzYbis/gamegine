// vlk_pipeline.cpp                                                   -*-C++-*-
#include <rhi/vlk/vlk_pipeline.h>

// std
#include <fstream>
#include <stdexcept>
#include <vector>

// rhi
#include <rhi/rhi_types.h>

// vlk
#include <rhi/vlk/vlk_context.h>
#include <rhi/vlk/vlk_resourcelayout.h>
#include <rhi/vlk/vlk_utils.h>

// core
#include <core/core_vertex.h>

namespace eng::rhi::vlk {

namespace {

vk::PolygonMode getVkPolygonMode(const PolygonMode mode)
{
    switch (mode) {
    case PolygonMode::Fill: return vk::PolygonMode::eFill;
    case PolygonMode::Line: return vk::PolygonMode::eLine;
    case PolygonMode::Point: return vk::PolygonMode::ePoint;
    default: return vk::PolygonMode::eFill;
    }
}

vk::CullModeFlags getVkCullMode(const CullMode mode)
{
    switch (mode) {
    case CullMode::None: return vk::CullModeFlagBits::eNone;
    case CullMode::Front: return vk::CullModeFlagBits::eFront;
    case CullMode::Back: return vk::CullModeFlagBits::eBack;
    default: return vk::CullModeFlagBits::eBack;
    }
}

std::vector<char> readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }
    std::vector<char> buffer((file.tellg()));
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    return buffer;
}

vk::raii::ShaderModule createShaderModule(const vk::raii::Device&  device,
                                          const std::vector<char>& code)
{
    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    return {device, createInfo};
}

vk::Format getVkVertexFormat(const VertexFormat format)
{
    switch (format) {
    case VertexFormat::Float1: return vk::Format::eR32Sfloat;
    case VertexFormat::Float2: return vk::Format::eR32G32Sfloat;
    case VertexFormat::Float3: return vk::Format::eR32G32B32Sfloat;
    case VertexFormat::Float4: return vk::Format::eR32G32B32A32Sfloat;
    case VertexFormat::Int1: return vk::Format::eR32Sint;
    case VertexFormat::Int2: return vk::Format::eR32G32Sint;
    case VertexFormat::Int3: return vk::Format::eR32G32B32Sint;
    case VertexFormat::Int4: return vk::Format::eR32G32B32A32Sint;
    default: return vk::Format::eUndefined;
    }
}

vk::VertexInputRate getVkVertexInputRate(const VertexInputRate rate)
{
    return rate == VertexInputRate::Vertex ? vk::VertexInputRate::eVertex
                                           : vk::VertexInputRate::eInstance;
}

vk::ShaderStageFlags getVkShaderStageFlags(const ShaderStage stage)
{
    switch (stage) {
    case ShaderStage::Vertex: return vk::ShaderStageFlagBits::eVertex;
    case ShaderStage::Fragment: return vk::ShaderStageFlagBits::eFragment;
    default: return vk::ShaderStageFlagBits::eAllGraphics;
    }
}

}  // close anonymous namespace

// --------------
// class Pipeline
// --------------

// CREATORS
Pipeline::Pipeline(Context* context, const PipelineConfig& config)
: d_context_p(context)
, d_layout(nullptr)
, d_pipeline(nullptr)
{
    // Load Shaders based on Config
    auto vertShaderCode = readFile("shaders/" + config.vertexShaderName +
                                   ".spv");
    auto fragShaderCode = readFile("shaders/" + config.fragmentShaderName +
                                   ".spv");

    vk::raii::ShaderModule vertShaderModule =
        createShaderModule(d_context_p->device(), vertShaderCode);
    vk::raii::ShaderModule fragShaderModule =
        createShaderModule(d_context_p->device(), fragShaderCode);

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo(
        {},
        vk::ShaderStageFlagBits::eVertex,
        *vertShaderModule,
        "vertMain");
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo(
        {},
        vk::ShaderStageFlagBits::eFragment,
        *fragShaderModule,
        "fragMain");

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                        fragShaderStageInfo};

    std::vector<vk::VertexInputBindingDescription> vkBindings;
    vkBindings.reserve(config.vertexBindings.size());
    for (const auto& [binding, stride, inputRate] : config.vertexBindings) {
        vkBindings.emplace_back(binding,
                                stride,
                                getVkVertexInputRate(inputRate));
    }

    std::vector<vk::VertexInputAttributeDescription> vkAttributes;
    vkAttributes.reserve(config.vertexAttributes.size());
    for (const auto& [location, binding, format, offset] :
         config.vertexAttributes) {
        vkAttributes.emplace_back(location,
                                  binding,
                                  getVkVertexFormat(format),
                                  offset);
    }

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly(
        {},
        vk::PrimitiveTopology::eTriangleList,
        vk::False);

    // Viewport & Scissor
    vk::PipelineViewportStateCreateInfo viewportState({},
                                                      1,
                                                      nullptr,
                                                      1,
                                                      nullptr);

    // Rasterizer
    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.polygonMode = getVkPolygonMode(config.polygonMode);
    rasterizer.cullMode    = getVkCullMode(config.cullMode);
    rasterizer.frontFace   = vk::FrontFace::eClockwise;
    rasterizer.lineWidth   = 1.0f;

    // Multisampling
    vk::PipelineMultisampleStateCreateInfo multisampling(
        {},
        d_context_p->msaaSamples());

    // Blending
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR |
                                          vk::ColorComponentFlagBits::eG |
                                          vk::ColorComponentFlagBits::eB |
                                          vk::ColorComponentFlagBits::eA;
    if (config.enableBlending) {
        colorBlendAttachment.blendEnable         = vk::True;
        colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
        colorBlendAttachment.dstColorBlendFactor =
            vk::BlendFactor::eOneMinusSrcAlpha;
        colorBlendAttachment.colorBlendOp        = vk::BlendOp::eAdd;
        colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
        colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
        colorBlendAttachment.alphaBlendOp        = vk::BlendOp::eAdd;
    }

    vk::PipelineColorBlendStateCreateInfo colorBlending({},
                                                        vk::False,
                                                        vk::LogicOp::eCopy,
                                                        1,
                                                        &colorBlendAttachment);

    // Dynamic States
    std::vector dynamicStates = {vk::DynamicState::eViewport,
                                 vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState({}, dynamicStates);

    // Depth Stencil
    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.depthTestEnable       = config.enableDepthTest;
    depthStencil.depthWriteEnable      = config.enableDepthWrite;
    depthStencil.depthCompareOp        = vk::CompareOp::eLess;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable     = VK_FALSE;

    std::vector<vk::DescriptorSetLayout> vkLayouts;
    vkLayouts.reserve(config.resourceLayouts.size());
    for (auto* layout : config.resourceLayouts) {
        vkLayouts.push_back(*static_cast<ResourceLayout*>(layout)->layout());
    }

    // Configure Push Constants
    vk::PushConstantRange pushRange{};
    if (config.pushConstantSize > 0) {
        pushRange.stageFlags = getVkShaderStageFlags(config.pushConstantStage);
        pushRange.offset     = 0;
        pushRange.size       = config.pushConstantSize;
    }

    // Create the Pipeline Layout
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(
        vkLayouts.size());
    pipelineLayoutInfo.pSetLayouts = vkLayouts.data();
    if (config.pushConstantSize > 0) {
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges    = &pushRange;
    }
    d_layout = vk::raii::PipelineLayout(context->device(), pipelineLayoutInfo);

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(
        vkBindings.size());
    vertexInputInfo.pVertexBindingDescriptions      = vkBindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(
        vkAttributes.size());
    vertexInputInfo.pVertexAttributeDescriptions = vkAttributes.data();

    // Dynamic Rendering Info
    vk::Format colorFormat = Utils::getVkFormat(config.colorAttachmentFormat);
    vk::Format depthFormat = Utils::getVkFormat(config.depthAttachmentFormat);

    vk::PipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.colorAttachmentCount    = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;
    renderingInfo.depthAttachmentFormat   = depthFormat;

    // Assemblage final
    vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
    graphicsPipelineCreateInfo.pNext               = &renderingInfo;
    graphicsPipelineCreateInfo.stageCount          = 2;
    graphicsPipelineCreateInfo.pStages             = shaderStages;
    graphicsPipelineCreateInfo.pVertexInputState   = &vertexInputInfo;
    graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    graphicsPipelineCreateInfo.pViewportState      = &viewportState;
    graphicsPipelineCreateInfo.pRasterizationState = &rasterizer;
    graphicsPipelineCreateInfo.pMultisampleState   = &multisampling;
    graphicsPipelineCreateInfo.pDepthStencilState  = &depthStencil;
    graphicsPipelineCreateInfo.pColorBlendState    = &colorBlending;
    graphicsPipelineCreateInfo.pDynamicState       = &dynamicState;
    graphicsPipelineCreateInfo.layout              = *d_layout;

    d_pipeline = vk::raii::Pipeline(d_context_p->device(),
                                    nullptr,
                                    graphicsPipelineCreateInfo);
}

}  // close package namespace
