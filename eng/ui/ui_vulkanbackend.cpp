// ui_vulkanbackend.cpp                                               -*-C++-*-
#include <ui/ui_vulkanbackend.h>

// rhi
#include <rhi/vlk/vlk_commandlist.h>
#include <rhi/vlk/vlk_swapchain.h>
#include <rhi/vlk/vlk_texture.h>

// third-party
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

namespace eng::ui {

// -------------------
// class VulkanBackend
// -------------------

// MANIPULATORS
void VulkanBackend::initialize(rhi::ContextProtocol*   context,
                               rhi::SwapchainProtocol* swapchain)
{
    const auto* vlkContext   = static_cast<rhi::vlk::Context*>(context);
    const auto* vlkSwapchain = static_cast<rhi::vlk::Swapchain*>(swapchain);

    // Before initializing ImGui, we need to create a DescriptorPool. This
    // pool allows ImGui to allocate the descriptors needed to display its
    // own resources. These resources are mainly the texture containing the
    // font, but also any custom images that we want to display in the UI.
    // The size of this pool (1000 for each type) is deliberately oversized
    // to ensure that we never run out of space when adding textures.
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        {vk::DescriptorType::eSampler, 1000},
        {vk::DescriptorType::eCombinedImageSampler, 1000},
        {vk::DescriptorType::eSampledImage, 1000},
        {vk::DescriptorType::eStorageImage, 1000},
        {vk::DescriptorType::eUniformTexelBuffer, 1000},
        {vk::DescriptorType::eStorageTexelBuffer, 1000},
        {vk::DescriptorType::eUniformBuffer, 1000},
        {vk::DescriptorType::eStorageBuffer, 1000},
        {vk::DescriptorType::eUniformBufferDynamic, 1000},
        {vk::DescriptorType::eStorageBufferDynamic, 1000},
        {vk::DescriptorType::eInputAttachment, 1000}};

    const vk::DescriptorPoolCreateInfo poolInfo(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        1000 * poolSizes.size(),
        poolSizes);

    // Create RAII DescriptorPool for automatic memory management.
    d_descriptorPool = vk::raii::DescriptorPool(vlkContext->device(),
                                                poolInfo);

    // Configure the Vulkan backend, give Vulkan handles (Instance, Device,
    // Queue)
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion                = VK_API_VERSION_1_3;
    init_info.Instance                  = *vlkContext->instance();
    init_info.PhysicalDevice            = *vlkContext->physicalDevice();
    init_info.Device                    = *vlkContext->device();
    init_info.QueueFamily    = vlkContext->graphicsQueueFamilyIndex();
    init_info.Queue          = *vlkContext->graphicsQueue();
    init_info.DescriptorPool = *d_descriptorPool;
    init_info.MinImageCount  = 3;
    init_info.ImageCount     = 3;

    // Since we are using dynamic rendering, we need to provide our
    // swapchain color format so that ImGui can generate its own internal
    // graphics pipeline, ensuring that it matches the screen's output
    // format exactly.
    const VkFormat colorFormat = static_cast<VkFormat>(
        vlkSwapchain->vlkFormat());

    init_info.UseDynamicRendering = true;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pNext = nullptr;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo
        .colorAttachmentCount = 1;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo
        .pColorAttachmentFormats = &colorFormat;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo
        .depthAttachmentFormat = static_cast<VkFormat>(
        vlkSwapchain->depthFormat());

    ImGui_ImplVulkan_Init(&init_info);
}

void VulkanBackend::newFrame()
{
    ImGui_ImplVulkan_NewFrame();
}

void VulkanBackend::renderDrawData(ImDrawData*               drawData,
                                   rhi::CommandListProtocol* cmd)
{
    const auto*             vlkCmd = static_cast<rhi::vlk::CommandList*>(cmd);
    const vk::CommandBuffer nativeCmdBuffer = *vlkCmd->commandBuffer();

    ImGui_ImplVulkan_RenderDrawData(drawData, nativeCmdBuffer);
}

ImTextureID VulkanBackend::registerTexture(rhi::TextureProtocol* texture)
{
    const auto* vlkTexture = static_cast<rhi::vlk::Texture*>(texture);

    const VkDescriptorSet descriptorSet = ImGui_ImplVulkan_AddTexture(
        *vlkTexture->sampler(),
        *vlkTexture->view(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    return reinterpret_cast<ImTextureID>(descriptorSet);
}

void VulkanBackend::removeTexture(const ImTextureID textureId)
{
    const auto descriptorSet = reinterpret_cast<VkDescriptorSet>(textureId);
    ImGui_ImplVulkan_RemoveTexture(descriptorSet);
}

void VulkanBackend::shutdown()
{
    ImGui_ImplVulkan_Shutdown();
}

}  // close package namespace
