// pnl_game.h                                                         -*-C++-*-
#ifndef INCLUDED_ENG_UI_PNL_GAME_H
#define INCLUDED_ENG_UI_PNL_GAME_H

//@PURPOSE: Provide a UI panel displaying the rendered game scene.
//
//@CLASSES:
//  eng::ui::pnl::GamePanel: Immediate-mode UI panel for game viewport
//  rendering.
//
//@DESCRIPTION: This component provides a concrete implementation of the
// 'eng::ui::UIPanel' interface. 'eng::ui::GamePanel' is responsible
// for rendering a Dear ImGui window that displays the output of the
// engine's renderer. It handles viewport focusing and resizing logic.

// core
#include <core/core_input.h>

// ui
#include <ui/ui_panel.h>

// third-party
#include <imgui.h>

// Forward declaration
namespace eng::rnd {
class Renderer;
}  // close package namespace

namespace eng::ui {
class UIManager;
}  // close package namespace

namespace eng::ui::pnl {

// ===============
// class GamePanel
// ===============

/// Immediate-mode UI panel for displaying the game scene.
class GamePanel : public UIPanel {
  private:
    // DATA

    /// Non-owning pointer to the UI manager for backend access.
    UIManager* d_manager_p = nullptr;

    /// Non-owning pointer to the renderer used to retrieve the scene image.
    rnd::Renderer* d_renderer_p = nullptr;

    /// Non-owning pointer to the input manager for viewport focus updates.
    core::InputManager* d_input_manager_p = nullptr;

    /// Registered ImGui texture handle for the game viewport.
    ImTextureID d_imguiTextureID = 0;

    /// Cached size of the viewport panel.
    ImVec2 d_viewportSize = {0.0f, 0.0f};

  public:
    // CREATORS

    /// Create a game panel using the specified 'manager', 'renderer' and
    /// 'inputManager'.
    explicit GamePanel(UIManager*          manager,
                       rnd::Renderer*      renderer,
                       core::InputManager* inputManager);

    /// Destroy this panel.
    ~GamePanel() override = default;

    // MANIPULATORS

    /// Render the contents of the game panel using Dear ImGui commands.
    void render() override;
};

}  // close package namespace
#endif  // INCLUDED_ENG_UI_PNL_GAME_H
