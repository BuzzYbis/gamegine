// entities_panel.h                                                   -*-C++-*-
#ifndef INCLUDED_ENGINE_UI_PANEL_ENTITIES_PANEL_H
#define INCLUDED_ENGINE_UI_PANEL_ENTITIES_PANEL_H

//@PURPOSE: Provide a UI panel listing all the entities in the current scene
//
//@CLASSES:
//  engine::ui::EntitiesPanel: Immediate-mode UI panel for listing all entities
//  in the curent scene
//
//@DESCRIPTION: This component provides a concrete implementation of the
// `engine::ui::UIPanel` interface. `engine::ui::EntitiesPanel` is responsible
// for rendering a Dear ImGui window that displays all entities in the current
// scene

// ui
#include <ui/ui_panel.h>

// Forward declaration
namespace engine::scene {
class Scene;
}  // close engine::scene namespace

namespace engine::ui {

class EntitiesPanel : public UIPanel {
  private:
    // DATA

    /// Pointer of the scene ; allow to retrieve all the entities
    scene::Scene* d_scene_p = nullptr;

  public:
    // CREATORS

    explicit EntitiesPanel(scene::Scene* scene);
    ~EntitiesPanel() override = default;

    // MANIPULATORS

    void render() override;
};

}  // close engine::ui namespace

#endif  // INCLUDED_ENGINE_UI_PANEL_ENTITIES_PANEL_H
