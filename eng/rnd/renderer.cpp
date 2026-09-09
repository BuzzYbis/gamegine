// renderer.cpp                                                       -*-C++-*-
#include <rnd/renderer.h>

// std
#include <iostream>

// core
#include <core/core_instance.h>
#include <core/core_log.h>
#include <core/core_vertex.h>

// renderer
#include <rnd/material.h>

// scene
#include <scn/comp/comp_camera.h>
#include <scn/comp/comp_mesh.h>
#include <scn/comp/comp_transform.h>

namespace eng::rnd {
namespace {
/// Return the name under which the pipeline of a material of the specified
/// 'mode' and 'doubleSided' is registered. Opaque and masked materials share
/// a name because they share their whole pipeline state.
std::string materialPipelineName(const AlphaMode mode, const bool doubleSided)
{
    const std::string blending = mode == AlphaMode::Blend ? "Blend" : "Opaque";
    const std::string faces    = doubleSided ? "_DoubleSided" : "";

    return "PBR_" + blending + faces;
}

struct UniformBufferObject {
    glm::mat4 viewProj;

    // The shading of a surface depends on the direction it is looked from,
    // which the view projection alone does not carry. The fourth component
    // is padding, a 'vec3' being aligned on 16 bytes in std140.
    glm::vec4 cameraPosition = glm::vec4(0.0f);
};
}

Renderer::Renderer(rhi::ContextProtocol* context, core::Window* window)
: d_window_p(window)
, d_context_p(context)
{
}

bool Renderer::initialize()
{
    d_swapchain = d_context_p->createSwapchain(d_window_p->width(),
                                               d_window_p->height());

    // Create the camera buffer
    d_cameraUbo = d_context_p->createBuffer(sizeof(UniformBufferObject),
                                            rhi::BufferUsage::Uniform);
    // Create buffer for shaders, uniform, texture (cpu to gpu shader)
    rhi::ResourceLayoutConfig layoutConfig{};
    // The vertex stage reads the view projection, the fragment stage the
    // camera position it needs to build the view vector.
    layoutConfig.bindings.push_back(
        {.binding = 0,
         .type    = rhi::ResourceType::UniformBuffer,
         .stage   = rhi::ShaderStage::VertexFragment});
    d_globalLayout = d_context_p->createResourceLayout(layoutConfig);

    d_globalResourceSet = d_context_p->createResourceSet(d_globalLayout.get());
    d_globalResourceSet->updateBuffer(0, d_cameraUbo.get(), 0, 0);

    // A material binds one texture per slot of 'TextureSlot', followed by the
    // uniform buffer holding its factors. Every slot is bound on every
    // material, a neutral image standing in for the textures it declares none
    // for, so that no descriptor of the set is ever left unwritten.
    rhi::ResourceLayoutConfig matLayoutConfig{};

    for (uint32_t slot = 0; slot < k_TEXTURE_SLOT_COUNT; ++slot) {
        matLayoutConfig.bindings.push_back(
            {.binding = slot,
             .type    = rhi::ResourceType::TextureSampler,
             .stage   = rhi::ShaderStage::Fragment});
    }

    matLayoutConfig.bindings.push_back(
        {.binding = k_PARAMS_BINDING,
         .type    = rhi::ResourceType::UniformBuffer,
         .stage   = rhi::ShaderStage::Fragment});

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
                     static_cast<float>(d_swapchain->height()),
                     0.0f,
                     1.0f);
    cmd->setScissor(0, 0, d_swapchain->width(), d_swapchain->height());

    return cmd;
}

void Renderer::beginSwapchainPass(rhi::CommandListProtocol* cmd) const
{
    constexpr rhi::ClearColor clearColor{.r = 0x13 / 255.f,  // 19
                                         .g = 0x12 / 255.f,  // 18
                                         .b = 0x10 / 255.f,  // 16
                                         .a = 1.0f};
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
    // A blended surface composites with what the target already holds and
    // writes no depth, so everything it is meant to be seen through has to
    // be drawn before it. That ordering is what the second pass buys.
    drawMeshes(cmd, scene, false);
    drawMeshes(cmd, scene, true);
}

void Renderer::drawMeshes(rhi::CommandListProtocol* cmd,
                          scn::Scene&               scene,
                          const bool                blended) const
{
    const auto group =
        scene.registry()
            .view<scn::comp::TransformComponent, scn::comp::MeshComponent>();

    for (const auto entity : group) {
        auto [transform, meshComp] =
            group.get<scn::comp::TransformComponent, scn::comp::MeshComponent>(
                entity);

        const glm::mat4 entityMatrix = transform.mat4();

        for (const MeshBatch& batch : meshComp.batches) {
            if (!batch.mesh || !batch.material) {
                continue;
            }

            // A mesh is drawn by the pass its own material belongs to, one
            // entity being free to hold meshes of both kinds.
            if ((batch.material->alphaMode() == AlphaMode::Blend) != blended) {
                continue;
            }

            // Everything below holds for the whole batch, whose placements
            // the device walks on its own, so nothing here is spent per
            // placement and the batch leaves as a single draw.

            // 1. Pipeline
            cmd->bindPipeline(batch.material->pipeline());

            // 2. Global Resources (Camera) on Slot 0
            cmd->bindResourceSet(batch.material->pipeline(),
                                 0,
                                 d_globalResourceSet.get());

            // 3. Material Resources (Textures) on Slot 1
            if (batch.material->resourceSet()) {
                cmd->bindResourceSet(batch.material->pipeline(),
                                     1,
                                     batch.material->resourceSet());
            }

            cmd->bindVertexBuffer(batch.instanceBuffer.get(),
                                  core::Instance::k_BINDING,
                                  0);

            // 4. Transformation matrix: the placement of the mesh inside
            // its model, carried into the world by the entity.
            rhi::MeshPushConstants constants{};
            constants.renderMatrix = entityMatrix;

            cmd->pushConstants(batch.material->pipeline(),
                               rhi::ShaderStage::Vertex,
                               0,
                               sizeof(rhi::MeshPushConstants),
                               &constants);

            // 5. Draw
            batch.mesh->draw(cmd,
                             static_cast<uint32_t>(batch.instances.size()));
        }
    }
}

void Renderer::updateUniformBuffer(scn::Scene& scene) const
{
    auto view           = glm::mat4(1.0f);
    auto proj           = glm::mat4(1.0f);
    auto cameraPosition = glm::vec3(0.0f);

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
        view           = scn::comp::CameraComponent::getViewMatrix(transform);
        proj           = camera.getProjection();
        cameraPosition = transform.d_translation;
        break;
    }
    UniformBufferObject ubo{};
    ubo.viewProj       = proj * view;
    ubo.cameraPosition = glm::vec4(cameraPosition, 1.0f);

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
    core::Instance::populatePipelineConfig(config);

    // One pipeline per state a material can ask for. Blending composites the
    // fragment with the destination, which only makes sense without depth
    // writes; a double sided material keeps both of its faces.
    for (const AlphaMode mode : {AlphaMode::Opaque, AlphaMode::Blend}) {
        for (const bool doubleSided : {false, true}) {
            rhi::PipelineConfig variant = config;

            if (mode == AlphaMode::Blend) {
                variant.enableBlending   = true;
                variant.enableDepthWrite = false;
            }

            if (doubleSided) {
                variant.cullMode = rhi::CullMode::None;
            }

            d_pipelines[materialPipelineName(mode, doubleSided)] =
                d_context_p->createPipeline(variant);
        }
    }

    rhi::PipelineConfig wireframeConfig = config;
    wireframeConfig.polygonMode         = rhi::PolygonMode::Line;
    wireframeConfig.cullMode            = rhi::CullMode::None;
    d_pipelines["Wireframe"] = d_context_p->createPipeline(wireframeConfig);

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

rhi::PipelineProtocol* Renderer::materialPipeline(const AlphaMode mode,
                                                  const bool doubleSided) const
{
    return getPipeline(materialPipelineName(mode, doubleSided));
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
