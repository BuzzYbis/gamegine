// pnl_game.cpp                                                       -*-C++-*-
#include <ui/pnl/pnl_game.h>

// renderer
#include <rnd/renderer.h>

// ui
#include <ui/ui_backendprotocol.h>
#include <ui/ui_manager.h>

// third-party
#include <imgui.h>

namespace eng::ui::pnl {

// ---------------
// class GamePanel
// ---------------

// CREATORS
GamePanel::GamePanel(UIManager*          manager,
                     rnd::Renderer*      renderer,
                     core::InputManager* inputManager)
: d_manager_p(manager)
, d_renderer_p(renderer)
, d_input_manager_p(inputManager)
{
}

// MANIPULATORS
void GamePanel::render()
{
    ImGui::Begin("Game");

    d_input_manager_p->setViewportFocused(ImGui::IsWindowFocused());
    d_input_manager_p->setViewportHovered(ImGui::IsWindowHovered());

    const ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    d_viewportSize                 = viewportPanelSize;

    // Register the render target texture with ImGui via the backend.
    if (d_imguiTextureID == 0 || d_renderer_p->consumeResizeEvent()) {
        if (d_imguiTextureID != 0) {
            d_manager_p->backend()->removeTexture(d_imguiTextureID);
            d_imguiTextureID = 0;
        }

        rhi::RenderTargetProtocol* target = d_renderer_p->viewRenderTarget();
        if (target && target->texture()) {
            d_imguiTextureID = d_manager_p->backend()->registerTexture(
                target->texture());
        }
    }

    // Render the viewport image.
    if (d_imguiTextureID) {
        ImGui::Image(d_imguiTextureID,
                     ImVec2{d_viewportSize.x, d_viewportSize.y});
    }
    else {
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

}  // close package namespace
