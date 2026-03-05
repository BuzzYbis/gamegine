// ui_panel.h                                                         -*-C++-*-
#ifndef INCLUDED_ENGINE_UI_UI_PANEL_H
#define INCLUDED_ENGINE_UI_UI_PANEL_H

//@PURPOSE: Provide an abstract base class for all UI panels.
//
//@CLASSES:
//  engine::ui::UIPanel: Protocol (interface) for rendering UI panels.
//
//@DESCRIPTION: This component provides a pure protocol, `engine::ui::UIPanel`,
// that defines the standard interface for all graphical user interface panels
// within the engine's editor or tools.
//
// Derived classes must implement the `render` method to dispatch Dear ImGui
// rendering commands. By relying on this interface, the UI manager can
// orchestrate the rendering of multiple disparate panels without knowing their
// concrete types.

namespace engine::ui {

// -------------
// class UIPanel
// -------------

/// Provide a protocol for rendering a single UI panel.
class UIPanel {
  public:
    // CREATORS

    /// Destroy this object.
    virtual ~UIPanel() = default;

    // MANIPULATORS

    /// Render the contents of this UI panel. This method must be implemented
    /// by derived classes to issue the appropriate ImGui commands.
    virtual void render() = 0;
};

}  // close engine::ui namespace

#endif  // INCLUDED_ENGINE_UI_UI_PANEL_H