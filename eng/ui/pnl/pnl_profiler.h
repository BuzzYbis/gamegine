// pnl_profiler.h                                                     -*-C++-*-
#ifndef INCLUDED_ENG_UI_PNL_PROFILER_H
#define INCLUDED_ENG_UI_PNL_PROFILER_H

//@PURPOSE: Provide a UI panel displaying the engine performance counters.
//
//@CLASSES:
//  eng::ui::ProfilerPanel: Immediate-mode UI panel showing frame statistics.
//
//@DESCRIPTION: This component provides a concrete implementation of the
// 'eng::ui::UIPanel' interface. 'eng::ui::ProfilerPanel' renders a Dear ImGui
// window reporting the frame rate, the frame time history, the per-scope CPU
// and GPU breakdown, and the per-frame counters gathered by
// 'eng::core::Profiler'.
//
// This panel only reads the profiler: it takes no measurement of its own. The
// figures it displays are those of the previous frame, since the panels are
// rendered before the current frame is committed.
//
// The textual figures are refreshed a few times per second rather than every
// frame: numbers flickering at the frame rate cannot be read. The plot, on
// the other hand, is redrawn every frame.

// core
#include <core/core_profiler.h>

// ui
#include <ui/ui_panel.h>

namespace eng::ui {

// ===================
// class ProfilerPanel
// ===================

/// Immediate-mode UI panel reporting the engine performance counters.
class ProfilerPanel : public UIPanel {
  private:
    // DATA

    /// Pointer to the profiler this panel reports on.
    core::Profiler* d_profiler_p = nullptr;

    /// Time, in seconds, since the textual figures were last refreshed.
    float d_refreshAccumulator = 0.0f;

    /// Copy of the frame whose figures are currently displayed. Holding a
    /// snapshot rather than reading the newest frame every time is what
    /// keeps the numbers readable.
    core::FrameRecord d_snapshot = {};

    /// Statistics of the frame duration as of the last refresh.
    core::ScopeStats d_frameStats = {};

    /// Whether the frame time plot is drawn on a fixed scale.
    bool d_fixedScale = true;

    /// Target frame budget, in milliseconds, above which figures turn red.
    float d_budgetMs = 16.67f;

  public:
    // CREATORS

    /// Create a panel reporting on the specified 'profiler'. The behavior is
    /// undefined unless 'profiler' remains valid for the lifetime of this
    /// object.
    explicit ProfilerPanel(core::Profiler* profiler);

    ~ProfilerPanel() override = default;

    // MANIPULATORS

    void render() override;
};

}  // close package namespace

#endif  // INCLUDED_ENG_UI_PNL_PROFILER_H
