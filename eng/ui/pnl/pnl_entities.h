// pnl_entities.h                                                     -*-C++-*-
#ifndef INCLUDED_ENG_UI_PNL_ENTITIES_H
#define INCLUDED_ENG_UI_PNL_ENTITIES_H

//@PURPOSE: Provide a UI panel listing all the entities in the current scene.
//
//@CLASSES:
//  eng::ui::pnl::EntitiesPanel: Immediate-mode UI panel for entity listing.
//
//@DESCRIPTION: This component provides a concrete implementation of the
// 'eng::ui::UIPanel' interface. 'eng::ui::EntitiesPanel' is responsible
// for rendering a Dear ImGui window that displays all entities present
// in the current scene registry.

// ui
#include <ui/ui_panel.h>

// Forward declaration
namespace eng::scn {
class Scene;
}  // close package namespace

namespace eng::ui::pnl {

// ===================
// class EntitiesPanel
// ===================

/// Immediate-mode UI panel for listing all entities in the current scene.
class EntitiesPanel : public UIPanel {
  private:
    // DATA

    /// Non-owning pointer to the scene; used to retrieve entities.
    scn::Scene* d_scene_p = nullptr;

  public:
    // CREATORS

    /// Create an entities panel for the specified 'scene'.
    explicit EntitiesPanel(scn::Scene* scene);

    /// Destroy this panel.
    ~EntitiesPanel() override = default;

    // MANIPULATORS

    /// Render the contents of the entities panel using Dear ImGui commands.
    void render() override;
};

}  // close package namespace
#endif  // INCLUDED_ENG_UI_PNL_ENTITIES_H
