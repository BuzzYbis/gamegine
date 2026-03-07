// renderer.cpp                                                       -*-C++-*-
#include <renderer/renderer.h>

// renderer
#include <renderer/texture.h>

// rhi
#include <rhi/vulkan/vk_context.h>
#include <rhi/vulkan/vk_resource_utils.h>
#include <rhi/vulkan/vk_types.h>

// scene
#include <scene/components/camera_component.h>
#include <scene/components/mesh_component.h>
#include <scene/components/transform_component.h>

// std
#include "core/log.h"

#include <iostream>

namespace engine::renderer {

Renderer::Renderer(core::Window* platform)
: d_window_p(platform)
, d_commandBuffers(nullptr)
{
}

Renderer::~Renderer()
{
    // Swapchain must be destroyed before the surface
    d_swapchain.reset();
    if (d_context) {
        vkDestroySurfaceKHR(*d_context->instance(),
                            *d_context->surface(),
                            nullptr);
    }
}

struct UniformBufferObject {
    glm::mat4 view;  // camera
    glm::mat4 proj;  // projection
};

bool Renderer::initialize(const bool enableValidationLayers)
{
    // Init Vulkan create instance
    d_context = std::make_unique<rhi::vulkan::VulkanContext>(d_window_p);
    if (!d_context->initialize(enableValidationLayers)) {
        std::cerr << "Failed to initialize the Vulkan context!" << std::endl;
        return false;
    }

    // Create swap chain
    d_swapchain = std::make_unique<rhi::vulkan::VulkanSwapchain>(
        *d_context,
        d_window_p->width(),
        d_window_p->height());

    if (!d_swapchain->initialize()) {
        std::cerr << "Failed to initialise the swapchain!" << std::endl;
        return false;
    }

    // Create a shared descriptor set layout for our shader.
    // Binding 0: UBO (vertex), Binding 1: texture (fragment)
    constexpr std::array bindings = {
        vk::DescriptorSetLayoutBinding(0,
                                       vk::DescriptorType::eUniformBuffer,
                                       1,
                                       vk::ShaderStageFlagBits::eVertex,
                                       nullptr),
        vk::DescriptorSetLayoutBinding(
            1,
            vk::DescriptorType::eCombinedImageSampler,
            1,
            vk::ShaderStageFlagBits::eFragment,
            nullptr)};
    const vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings);
    d_descriptorSetLayout = vk::raii::DescriptorSetLayout(d_context->device(),
                                                          layoutInfo);
    d_descriptorManager   = std::make_unique<rhi::vulkan::DescriptorManager>(
        *d_context,
        *d_swapchain,
        d_descriptorSetLayout,
        sizeof(UniformBufferObject));

    createPipelines();

    // Command Pool (memory gestion for the buffers)
    d_commandPool = std::make_unique<CommandPool>(*d_context);
    // Allocate Command Buffers (one per swapchain image)
    const auto imageCount = static_cast<uint32_t>(
        d_swapchain->imageViews().size());
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool        = *d_commandPool->commandPool();
    allocInfo.level              = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = imageCount;
    d_commandBuffers = vk::raii::CommandBuffers(d_context->device(),
                                                allocInfo);
    createSyncObjects();
    d_renderTarget = std::make_unique<rhi::vulkan::VulkanRenderTarget>(
        *d_context,
        d_swapchain->extent(),
        d_swapchain->format(),
        *d_commandPool->commandPool());

    return true;
}

void Renderer::createSyncObjects()
{
    const auto imageCount = static_cast<uint32_t>(
        d_swapchain->imageViews().size());
    d_imageAvailableSemaphores.clear();
    d_renderFinishedSemaphores.clear();
    d_inFlightFences.clear();
    d_imageAvailableSemaphores.reserve(imageCount);
    d_renderFinishedSemaphores.reserve(imageCount);
    d_inFlightFences.reserve(imageCount);

    for (uint32_t i = 0; i < imageCount; ++i) {
        d_imageAvailableSemaphores.emplace_back(d_context->device(),
                                                vk::SemaphoreCreateInfo{});
        d_renderFinishedSemaphores.emplace_back(d_context->device(),
                                                vk::SemaphoreCreateInfo{});
        d_inFlightFences.emplace_back(
            d_context->device(),
            vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
    }
}

vk::CommandBuffer Renderer::beginFrame(scene::Scene& scene)
{
    // Wait for the previous frame to finish
    const auto fenceResult = d_context->device().waitForFences(
        {*d_inFlightFences[d_currentFrame]},
        VK_TRUE,
        UINT64_MAX);
    if (fenceResult != vk::Result::eSuccess) {
        throw std::runtime_error("failed to wait for fence!");
    }

    // Acquire next image from swapchain
    if (!d_swapchain->acquireNextImage(
            d_imageAvailableSemaphores[d_currentFrame],
            d_imageIndex)) {
        // The swapchain has been recreated or has failed, we skip this frame.
        return nullptr;
    }
    updateUniformBuffer(d_imageIndex, scene);
    d_context->device().resetFences({*d_inFlightFences[d_currentFrame]});
    vk::CommandBuffer cmd = d_commandBuffers[d_imageIndex];
    cmd.reset();
    constexpr vk::CommandBufferBeginInfo beginInfo{};
    cmd.begin(beginInfo);
    vk::Viewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(d_swapchain->extent().width);
    viewport.height   = static_cast<float>(d_swapchain->extent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    cmd.setViewport(0, viewport);
    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent = d_swapchain->extent();
    cmd.setScissor(0, scissor);
    rhi::vulkan::VulkanResourceUtils::transition_image_layout(
        cmd,
        d_renderTarget->image(),
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},                                                  // srcAccessMask
        vk::AccessFlagBits2::eColorAttachmentWrite,          // dstAccessMask
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,  // srcStage
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,  // dstStage
        vk::ImageAspectFlagBits::eColor);
    vk::ImageAspectFlags depthAspect = vk::ImageAspectFlagBits::eDepth;
    if (d_swapchain->depthFormat() == vk::Format::eD32SfloatS8Uint ||
        d_swapchain->depthFormat() == vk::Format::eD24UnormS8Uint ||
        d_swapchain->depthFormat() == vk::Format::eD16UnormS8Uint) {
        depthAspect |= vk::ImageAspectFlagBits::eStencil;
    }
    rhi::vulkan::VulkanResourceUtils::transition_image_layout(
        cmd,
        *d_swapchain->depthImage(),
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthStencilAttachmentOptimal,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests |
            vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests |
            vk::PipelineStageFlagBits2::eLateFragmentTests,
        depthAspect);
    const bool useMsaa = d_context->msaaSamples() !=
                         vk::SampleCountFlagBits::e1;
    vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.imageView   = d_renderTarget->view();
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp      = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp     = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue  = clearColor;
    if (useMsaa) {
        colorAttachment.resolveMode      = vk::ResolveModeFlagBits::eAverage;
        colorAttachment.resolveImageView = d_renderTarget->msaaView();
        colorAttachment.resolveImageLayout =
            vk::ImageLayout::eColorAttachmentOptimal;
    }
    vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);
    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.imageView = *d_swapchain->depthImageView();
    depthAttachment.imageLayout =
        vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depthAttachment.loadOp     = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp    = vk::AttachmentStoreOp::eDontCare;
    depthAttachment.clearValue = clearDepth;
    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea = vk::Rect2D{{0, 0}, d_renderTarget->extent()};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments    = &colorAttachment;
    renderingInfo.pDepthAttachment     = &depthAttachment;
    cmd.beginRendering(renderingInfo);
    return cmd;
}

void Renderer::renderScene(const vk::CommandBuffer cmd,
                           scene::Scene&           scene) const
{
    const auto group =
        scene.registry()
            .view<scene::TransformComponent, scene::MeshComponent>();

    for (const auto entity : group) {
        auto [transform, meshComp] =
            group.get<scene::TransformComponent, scene::MeshComponent>(entity);

        if (meshComp.d_meshes.empty() ||
            meshComp.d_materials.size() != meshComp.d_meshes.size()) {
            continue;
        }

        MeshPushConstants constants{};
        constants.renderMatrix = transform.mat4();
        cmd.pushConstants(*meshComp.d_materials[0]->layout(),
                          vk::ShaderStageFlagBits::eVertex,
                          0,
                          sizeof(MeshPushConstants),
                          &constants);

        for (size_t i = 0; i < meshComp.d_meshes.size(); ++i) {
            const auto&     meshPtr  = meshComp.d_meshes[i];
            const Material* material = meshComp.d_materials[i];

            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics,
                             *material->pipeline());

            vk::DescriptorSet matSet = material->descriptorSet();

            if (!matSet && material->texture()) {
                matSet = *d_descriptorManager->getDescriptorSet(
                    d_imageIndex,
                    material->texture()->getImageView(),
                    material->texture()->getSampler());
            }

            if (matSet) {
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                       *material->layout(),
                                       0,
                                       matSet,
                                       {});
            }

            meshPtr->draw(cmd);
        }
    }
}

void Renderer::endFrame(const vk::CommandBuffer cmd)
{
    cmd.endRendering();
    rhi::vulkan::VulkanResourceUtils::transition_image_layout(
        cmd,
        d_swapchain->images()[d_imageIndex],
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        {},
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eBottomOfPipe,
        vk::ImageAspectFlagBits::eColor);

    cmd.end();

    constexpr vk::PipelineStageFlags waitStages[] = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::SubmitInfo submitInfo{};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &*d_imageAvailableSemaphores[d_currentFrame];
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores =
        &*d_renderFinishedSemaphores[d_currentFrame];
    d_context->graphicsQueue().submit(submitInfo,
                                      *d_inFlightFences[d_currentFrame]);
    vk::PresentInfoKHR presentInfo{};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &*d_renderFinishedSemaphores[d_currentFrame];
    presentInfo.swapchainCount  = 1;
    const vk::SwapchainKHR swapchain = *d_swapchain->swapchain();
    presentInfo.pSwapchains          = &swapchain;
    presentInfo.pImageIndices        = &d_imageIndex;
    const auto presentResult         = d_context->graphicsQueue().presentKHR(
        presentInfo);
    if (presentResult == vk::Result::eErrorOutOfDateKHR ||
        presentResult == vk::Result::eSuboptimalKHR) {
        d_swapchain->recreateSwapChain();

        d_renderTarget = std::make_unique<rhi::vulkan::VulkanRenderTarget>(
            *d_context,
            d_swapchain->extent(),
            d_swapchain->format(),
            *d_commandPool->commandPool());

        d_wasResized = true;
    }
    else {
        assert(presentResult == vk::Result::eSuccess);
    }
    d_currentFrame = (d_currentFrame + 1) %
                     static_cast<uint32_t>(d_inFlightFences.size());
}

void Renderer::beginSwapchainPass(const vk::CommandBuffer cmd)
{
    cmd.endRendering();
    rhi::vulkan::VulkanResourceUtils::transition_image_layout(
        cmd,
        d_renderTarget->image(),
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::AccessFlagBits2::eShaderRead,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eFragmentShader,
        vk::ImageAspectFlagBits::eColor);

    const vk::Image swapchainImage = d_swapchain->images()[d_imageIndex];
    rhi::vulkan::VulkanResourceUtils::transition_image_layout(
        cmd,
        swapchainImage,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eTopOfPipe,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::ImageAspectFlagBits::eColor);

    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.imageView   = *d_swapchain->imageViews()[d_imageIndex];
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp      = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp     = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue  = vk::ClearColorValue(0.1f, 0.1f, 0.1f, 1.0f);

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea = vk::Rect2D{{0, 0}, d_swapchain->extent()};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments    = &colorAttachment;

    cmd.beginRendering(renderingInfo);
}

void Renderer::updateUniformBuffer(const uint32_t currentImage,
                                   scene::Scene&  scene) const
{
    auto view = glm::mat4(1.0f);
    auto proj = glm::mat4(1.0f);

    const auto extent = d_swapchain->extent();
    const auto width  = static_cast<float>(extent.width);
    const auto height = static_cast<float>(extent.height);

    if (width <= 0.0f || height <= 0.0f) {
        return;
    }

    const auto cameraView =
        scene.registry()
            .view<scene::TransformComponent, scene::CameraComponent>();

    for (const auto entity : cameraView) {
        auto [transform, camera] =
            cameraView.get<scene::TransformComponent, scene::CameraComponent>(
                entity);
        camera.setAspectRatio(width / height);
        view = camera.getViewMatrix(transform);
        proj = camera.getProjection();

        break;
    }
    UniformBufferObject ubo{};
    ubo.view = view;
    ubo.proj = proj;
    d_descriptorManager->updateUniformBuffer(currentImage, &ubo);
}

void Renderer::createPipelines()
{
    d_pipelines["PBR_Opaque"] = std::make_unique<rhi::vulkan::VulkanPipeline>(
        *d_context,
        d_swapchain->format(),
        d_swapchain->depthFormat(),
        *d_descriptorSetLayout);

    rhi::vulkan::PipelineConfig wireframeConfig{};
    wireframeConfig.polygonMode = vk::PolygonMode::eLine;
    wireframeConfig.cullMode    = vk::CullModeFlagBits::eNone;

    d_pipelines["Wireframe"] = std::make_unique<rhi::vulkan::VulkanPipeline>(
        *d_context,
        d_swapchain->format(),
        d_swapchain->depthFormat(),
        *d_descriptorSetLayout,
        wireframeConfig);

    rhi::vulkan::PipelineConfig transparentConfig{};
    transparentConfig.enableBlending   = true;
    transparentConfig.enableDepthWrite = false;

    d_pipelines["Transparent"] = std::make_unique<rhi::vulkan::VulkanPipeline>(
        *d_context,
        d_swapchain->format(),
        d_swapchain->depthFormat(),
        *d_descriptorSetLayout,
        transparentConfig);
}

rhi::vulkan::VulkanPipeline*
Renderer::getPipeline(const std::string& name) const
{
    const auto it = d_pipelines.find(name);
    if (it != d_pipelines.end()) {
        return it->second.get();
    }

    std::cerr << "[Renderer] Error : Pipeline '" << name << "' not found !"
              << std::endl;
    return nullptr;
}

}  // close engine::renderer namespace