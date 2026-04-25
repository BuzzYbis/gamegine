// renderer.cpp                                                       -*-C++-*-
#include <rnd/renderer.h>

// std
#include <iostream>

// core
#include <core/core_log.h>

// scene
#include <scn/comp/comp_camera.h>
#include <scn/comp/comp_mesh.h>
#include <scn/comp/comp_transform.h>

namespace eng::rnd {

struct UniformBufferObject {
    glm::mat4 viewProj;
};

Renderer::Renderer(rhi::ContextProtocol* context, core::Window* window)
: d_window_p(window)
, d_context_p(context)
{
}

bool Renderer::initialize()
{
    d_swapchain = d_context_p->createSwapchain(d_window_p->width(),
                                               d_window_p->height());

    d_cameraUbo = d_context_p->createBuffer(sizeof(UniformBufferObject),
                                            rhi::BufferUsage::Uniform);

    rhi::ResourceLayoutConfig layoutConfig{};
    layoutConfig.bindings.push_back(
        {0, rhi::ResourceType::UniformBuffer, rhi::ShaderStage::Vertex});
    d_globalLayout = d_context_p->createResourceLayout(layoutConfig);

    d_globalResourceSet = d_context_p->createResourceSet(d_globalLayout.get());
    d_globalResourceSet->updateBuffer(0, d_cameraUbo.get(), 0, 0);

    rhi::ResourceLayoutConfig matLayoutConfig{};
    matLayoutConfig.bindings.push_back(
        {0, rhi::ResourceType::TextureSampler, rhi::ShaderStage::Fragment});
    d_materialLayout = d_context_p->createResourceLayout(matLayoutConfig);

    createPipelines();

    constexpr uint32_t framesInFlight = 3;
    for (uint32_t i = 0; i < framesInFlight; ++i) {
        d_commandLists.push_back(d_context_p->createCommandList());
    }

    return true;
}

rhi::CommandListProtocol* Renderer::beginFrame(scn::Scene& scene)
{
    const auto imageIndexOpt = d_swapchain->acquireNextImage();

    if (!imageIndexOpt.has_value()) {
        d_wasResized = true;
        return nullptr;
    }

    d_imageIndex = imageIndexOpt.value();

    updateUniformBuffer(scene);

    rhi::CommandListProtocol* cmd = d_commandLists[d_currentFrame].get();
    cmd->begin();

    cmd->setViewport(0.0f,
                     0.0f,
                     static_cast<float>(d_swapchain->width()),
                     static_cast<float>(d_swapchain->height()));
    cmd->setScissor(0, 0, d_swapchain->width(), d_swapchain->height());

    return cmd;
}

void Renderer::beginSwapchainPass(rhi::CommandListProtocol* cmd)
{
    constexpr rhi::ClearColor clearColor{0.1f, 0.1f, 0.1f, 1.0f};
    cmd->beginSwapchainRendering(d_swapchain.get(), d_imageIndex, clearColor);
}

void Renderer::endFrame(rhi::CommandListProtocol* cmd)
{
    cmd->endSwapchainRendering(d_swapchain.get(), d_imageIndex);
    cmd->end();

    const bool success = d_swapchain->submitAndPresent(cmd, d_imageIndex);

    if (!success) {
        d_wasResized = true;
    }

    d_currentFrame = (d_currentFrame + 1) % d_commandLists.size();
}

void Renderer::renderScene(rhi::CommandListProtocol* cmd,
                           scn::Scene&               scene) const
{
    const auto group =
        scene.registry()
            .view<scn::comp::TransformComponent, scn::comp::MeshComponent>();

    for (const auto entity : group) {
        auto [transform, meshComp] =
            group.get<scn::comp::TransformComponent, scn::comp::MeshComponent>(
                entity);

        if (meshComp.d_meshes.empty() ||
            meshComp.d_materials.size() != meshComp.d_meshes.size()) {
            continue;
        }

        rhi::MeshPushConstants constants{};
        constants.renderMatrix = transform.mat4();

        for (size_t i = 0; i < meshComp.d_meshes.size(); ++i) {
            const auto&     meshPtr  = meshComp.d_meshes[i];
            const Material* material = meshComp.d_materials[i];

            // 1. Pipeline
            cmd->bindPipeline(material->pipeline());

            // 2. Global Resources (Camera) on Slot 0
            cmd->bindResourceSet(material->pipeline(),
                                 0,
                                 d_globalResourceSet.get());

            // 3. Transformation matrix
            cmd->pushConstants(material->pipeline(),
                               rhi::ShaderStage::Vertex,
                               0,
                               sizeof(rhi::MeshPushConstants),
                               &constants);

            // 4. Material Resources (Textures) on Slot 1
            if (material->resourceSet()) {
                cmd->bindResourceSet(material->pipeline(),
                                     1,
                                     material->resourceSet());
            }

            // 5. Draw
            meshPtr->draw(cmd);
        }
    }
}

void Renderer::updateUniformBuffer(scn::Scene& scene) const
{
    auto view = glm::mat4(1.0f);
    auto proj = glm::mat4(1.0f);

    const auto width  = static_cast<float>(d_swapchain->width());
    const auto height = static_cast<float>(d_swapchain->height());

    if (width <= 0.0f || height <= 0.0f) {
        return;
    }

    const auto cameraView =
        scene.registry()
            .view<scn::comp::TransformComponent, scn::comp::CameraComponent>();

    for (const auto entity : cameraView) {
        auto [transform,
              camera] = cameraView.get<scn::comp::TransformComponent,
                                       scn::comp::CameraComponent>(entity);
        camera.setAspectRatio(width / height);
        view = scn::comp::CameraComponent::getViewMatrix(transform);
        proj = camera.getProjection();
        break;
    }
    UniformBufferObject ubo{};
    ubo.viewProj = proj * view;

    d_cameraUbo->uploadData(&ubo, sizeof(UniformBufferObject), 0);
}

void Renderer::createPipelines()
{
    rhi::PipelineConfig config{};
    config.vertexShaderName      = "shader";
    config.fragmentShaderName    = "shader";
    config.colorAttachmentFormat = d_swapchain->format();
    config.depthAttachmentFormat = rhi::Format::D32_SFloat;

    config.resourceLayouts   = {d_globalLayout.get(), d_materialLayout.get()};
    config.pushConstantSize  = sizeof(rhi::MeshPushConstants);
    config.pushConstantStage = rhi::ShaderStage::Vertex;

    core::Vertex::populatePipelineConfig(config);

    d_pipelines["PBR_Opaque"] = d_context_p->createPipeline(config);

    rhi::PipelineConfig wireframeConfig = config;
    wireframeConfig.polygonMode         = rhi::PolygonMode::Line;
    wireframeConfig.cullMode            = rhi::CullMode::None;
    d_pipelines["Wireframe"] = d_context_p->createPipeline(wireframeConfig);

    rhi::PipelineConfig transparentConfig = config;
    transparentConfig.enableBlending      = true;
    transparentConfig.enableDepthWrite    = false;
    d_pipelines["Transparent"]            = d_context_p->createPipeline(
        transparentConfig);

    // Dedicated UI Pipeline for Swapchain pass
    rhi::PipelineConfig uiConfig = config;
    uiConfig.colorAttachmentFormat =
        d_swapchain->format();  // Use swapchain format
    uiConfig.enableBlending   = true;
    uiConfig.enableDepthWrite = false;
    uiConfig.enableDepthTest  = false;
    d_pipelines["UI"]         = d_context_p->createPipeline(uiConfig);
}

rhi::PipelineProtocol* Renderer::getPipeline(const std::string& name) const
{
    const auto it = d_pipelines.find(name);
    if (it != d_pipelines.end()) {
        return it->second.get();
    }

    std::cerr << "[Renderer] Error : Pipeline '" << name << "' not found !"
              << std::endl;
    return nullptr;
}

bool Renderer::consumeResizeEvent()
{
    if (d_wasResized) {
        d_wasResized = false;
        return true;
    }
    return false;
}

}  // close package namespace
