// ui_context.cpp                                                     -*-C++-*-
#include <ui/ui_context.h>

// imgui
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>

namespace engine::ui {
void UIContext::initialize(const rhi::vulkan::VulkanContext&   context,
                           const rhi::vulkan::VulkanSwapchain& swapchain,
                           const core::Window&                 platform)
{
    // Before initializing ImGui, we need to create a DescriptorPool. This pool
    // allows ImGui to allocate the descriptors needed to display its own
    // resources. These resources are mainly the texture containing the font,
    // but also any custom images that we want to display in the UI. The size
    // of this pool (1000 for each type) is deliberately oversized to ensure
    // that we never run out of space when adding textures.
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
    d_descriptorPool = vk::raii::DescriptorPool(context.device(), poolInfo);

    // ImGui context, allocate internal memory for ImGui.
    IMGUI_CHECKVERSION();
    d_context_p = ImGui::CreateContext();

    // Enable global features.
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable keyboard
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable docking

    ImGui::StyleColorsDark();

    // Install the necessary hooks to capture window events (mouse clicks,
    // keyboard strokes, resizing).
    ImGui_ImplGlfw_InitForVulkan(platform.window(), true);

    // Configure the Vulkan backend, give Vulkan handles (Instance, Device,
    // Queue)
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion                = VK_API_VERSION_1_3;
    init_info.Instance                  = *context.instance();
    init_info.PhysicalDevice            = *context.physicalDevice();
    init_info.Device                    = *context.device();
    init_info.QueueFamily               = context.graphicsQueueFamilyIndex();
    init_info.Queue                     = *context.graphicsQueue();
    init_info.DescriptorPool            = *d_descriptorPool;
    init_info.MinImageCount             = 3;
    init_info.ImageCount                = 3;

    // Since we are using dynamic rendering, we need to provide our swapchain
    // color format so that ImGui can generate its own internal graphics
    // pipeline, ensuring that it matches the screen's output format exactly.
    const auto colorFormat = static_cast<VkFormat>(swapchain.format());

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
        swapchain.depthFormat());

    ImGui_ImplVulkan_Init(&init_info);
}

void UIContext::beginFrame()
{
    // Requests the Vulkan backend to prepare the ImGui resources (for example,
    // recreate the font texture if it has changed).
    ImGui_ImplVulkan_NewFrame();

    // Updates the status of user inputs by reading data from GLFW (mouse
    // position, pressed keys).
    ImGui_ImplGlfw_NewFrame();

    // Collect all the data obtained by the above functions and indicate that
    // ImGui is ready to receive the UI declaration.
    ImGui::NewFrame();

    const ImGuiID        dockspaceId = ImGui::GetID("My Dockspace");
    const ImGuiViewport* viewport    = ImGui::GetMainViewport();
    if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr) {
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->Size);
        ImGuiID dock_id_left = 0;
        ImGuiID dock_id_main = dockspaceId;
        ImGui::DockBuilderSplitNode(dock_id_main,
                                    ImGuiDir_Left,
                                    0.20f,
                                    &dock_id_left,
                                    &dock_id_main);
        ImGuiID dock_id_left_top    = 0;
        ImGuiID dock_id_left_bottom = 0;
        ImGui::DockBuilderSplitNode(dock_id_left,
                                    ImGuiDir_Up,
                                    0.50f,
                                    &dock_id_left_top,
                                    &dock_id_left_bottom);
        ImGui::DockBuilderDockWindow("Game", dock_id_main);
        ImGui::DockBuilderDockWindow("Properties", dock_id_left_top);
        ImGui::DockBuilderDockWindow("Scene", dock_id_left_bottom);
        ImGui::DockBuilderFinish(dockspaceId);
    }

    // Submit dockspace
    ImGui::DockSpaceOverViewport(dockspaceId,
                                 viewport,
                                 ImGuiDockNodeFlags_PassthruCentralNode);
}

void UIContext::cleanUpFrame()
{
    // End the ImGui frame without generating rendering commands.
    ImGui::EndFrame();
}

void UIContext::endFrame(const vk::CommandBuffer cmd)
{
    // Compile all the UI declarations into optimized vertex and index lists,
    // stored in an `ImDrawData` object.
    ImGui::Render();

    // The Vulkan backend reads this ImDrawData, binds its internal graphics
    // pipeline, updates its vertex/index buffers, and records the drawing
    // commands directly in the CommandBuffer.
    ImDrawData* drawData = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
}

void UIContext::shutdown()
{
    // Shutdown Vulkan backend and GLFW.
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    // Delete the ImGui context
    ImGui::DestroyContext(d_context_p);

    // The DescriptorPool will be automatically deleted by RAII
}
}  // close engine::ui namespace