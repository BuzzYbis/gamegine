// ui_context.cpp                                                     -*-C++-*-
#include <ui/ui_context.h>

// core
#include <core/core_window.h>

// rhi
#include <rhi/rhi_commandlistprotocol.h>

// ui
#include <ui/ui_backendprotocol.h>

// third-party
#include <imgui_impl_glfw.h>
#include <imgui_internal.h>

namespace eng::ui {

// ---------------
// class UIContext
// ---------------

// MANIPULATORS
void UIContext::initialize(rhi::ContextProtocol*                   context,
                           rhi::SwapchainProtocol*                 swapchain,
                           const core::Window&                     platform,
                           const std::unique_ptr<BackendProtocol>& backend)
{
    d_backend_p = backend.get();

    // ImGui context, allocate internal memory for ImGui.
    IMGUI_CHECKVERSION();
    d_context_p = ImGui::CreateContext();

    // Enable global features.
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable keyboard
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable docking

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOther(platform.window(), true);

    d_backend_p->initialize(context, swapchain);
}

void UIContext::beginFrame()
{
    // Requests the API backend to prepare the ImGui resources (for example,
    // recreate the font texture if it has changed).
    d_backend_p->newFrame();

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

void UIContext::endFrame(rhi::CommandListProtocol* cmd)
{
    // Compile all the UI declarations into optimized vertex and index lists,
    // stored in an `ImDrawData` object.
    ImGui::Render();

    // The API backend reads this ImDrawData, binds its internal graphics
    // pipeline, updates its vertex/index buffers, and records the drawing
    // commands directly in the CommandBuffer.
    ImDrawData* drawData = ImGui::GetDrawData();

    d_backend_p->renderDrawData(drawData, cmd);
}

void UIContext::shutdown()
{
    // Shutdown API backend and GLFW.
    d_backend_p->shutdown();
    ImGui_ImplGlfw_Shutdown();

    // Delete the ImGui context
    ImGui::DestroyContext(d_context_p);

    // The DescriptorPool will be automatically deleted by RAII
}
}  // close package namespace