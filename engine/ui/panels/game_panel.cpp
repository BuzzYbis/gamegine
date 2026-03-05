// game_panel.cpp                                                     -*-C++-*-
#include <ui/panels/game_panel.h>

// renderer
#include <renderer/renderer.h>

// imgui
#include <imgui.h>
#include <imgui_impl_vulkan.h>

namespace engine::ui {

GamePanel::GamePanel(renderer::Renderer* renderer,
                     core::InputManager* inputManager)
: d_renderer_p(renderer)
, d_input_manager_p(inputManager)
{
}

void GamePanel::render()
{
    ImGui::Begin("Game");

    d_input_manager_p->setViewportFocused(ImGui::IsWindowFocused());
    d_input_manager_p->setViewportHovered(ImGui::IsWindowHovered());

    const ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    d_viewportSize                 = viewportPanelSize;

    if (d_imguiTextureID) {
        ImGui::Image(d_imguiTextureID,
                     ImVec2{d_viewportSize.x, d_viewportSize.y});
    }
    else {
        const vk::ImageView view    = d_renderer_p->renderTargetView();
        const vk::Sampler   sampler = d_renderer_p->renderTargetSampler();

        if (view && sampler) {
            d_imguiTextureID = ImGui_ImplVulkan_AddTexture(
                sampler,
                view,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        ImGui::Text("Target render not initialized");
    }

    const ImVec2 windowPos  = ImGui::GetWindowPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();

    const auto overlayPos = ImVec2(windowPos.x + 10.0f,
                                   windowPos.y + windowSize.y - 10.0f);

    ImGui::SetNextWindowPos(overlayPos, ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.35f);

    constexpr ImGuiWindowFlags overlayFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove;

    ImGui::Begin("##FPS_Overlay", nullptr, overlayFlags);

    const float fps       = ImGui::GetIO().Framerate;
    const float frameTime = 1000.0f / fps;
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "FPS: %.1f", fps);
    ImGui::Text("Frame Time: %.3f ms", frameTime);

    ImGui::End();
    ImGui::End();
}

}  // close engine::ui namespace
