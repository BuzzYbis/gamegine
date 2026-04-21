// ui_panel.h                                                         -*-C++-*-
#ifndef INCLUDED_ENG_UI_UI_PANEL_H
#define INCLUDED_ENG_UI_UI_PANEL_H

//@PURPOSE: Provide a protocol for UI panel components.
//
//@CLASSES:
//  eng::ui::UIPanel: Interface for rendering a single immediate-mode UI panel.
//
//@DESCRIPTION: This component provides a pure protocol, 'eng::ui::UIPanel',
// that defines the standard interface for all graphical user interface panels
// within the engine's editor or tools. Derived classes must implement the
// 'render' method to issue Dear ImGui commands.

namespace eng::ui {

// =============
// class UIPanel
// =============

/// Interface for rendering a single immediate-mode UI panel.
class UIPanel {
  public:
    // CREATORS

    /// Destroy this panel.
    virtual ~UIPanel() = default;

    // MANIPULATORS

    /// Render the contents of this UI panel using Dear ImGui commands.
    virtual void render() = 0;
};

}  // close package namespace

#endif  // INCLUDED_ENG_UI_UI_PANEL_H