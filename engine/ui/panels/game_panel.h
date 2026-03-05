// game_panel.h                                                       -*-C++-*-
#ifndef INCLUDED_ENGINE_UI_PANEL_GAME_PANEL_H
#define INCLUDED_ENGINE_UI_PANEL_GAME_PANEL_H

//@PURPOSE: Provide a UI panel displaying the game.
//
//@CLASSES:
//  engine::ui::GamePanel: Immediate-mode UI panel for displaying the game.
//
//@DESCRIPTION: This component provides a concrete implementation of the
// `engine::ui::UIPanel` interface. `engine::ui::GamePanel` is responsible
// for rendering a Dear ImGui window that displays the game.

// core
#include <core/input.h>

// rhi
#include <rhi/vulkan/vk_descriptor_manager.h>

// ui
#include <ui/ui_panel.h>

// imgui
#include <imgui.h>

// Forward declaration
namespace engine::renderer {
class Renderer;
}  // close engine::renderer namespace

namespace engine::ui {

// ----------------
// class GamePanel
// ----------------

/// Immediate-mode UI panel for displaying the game.
class GamePanel : public UIPanel {
  private:
    // DATA

    /// Pointer to the renderer which is used to render the game.
    renderer::Renderer* d_renderer_p      = nullptr;

    /// Pointer to the input manager ; allows to handle inputs from the user.
    core::InputManager* d_input_manager_p = nullptr;
    
    /// Descriptor set for the ImGui texture. This is used to display the game in the UI.
    VkDescriptorSet     d_imguiTextureID  = nullptr;
    
    /// Size of the viewport.
    ImVec2              d_viewportSize    = {0.0f, 0.0f};

  public:
    // CREATORS

    explicit GamePanel(renderer::Renderer* renderer,
                       core::InputManager* inputManager);
    ~GamePanel() override = default;

    // MANIPULATORS

    void render() override;
};

}  // close engine::ui namespace

#endif  // INCLUDED_ENGINE_UI_PANEL_GAME_PANEL_H