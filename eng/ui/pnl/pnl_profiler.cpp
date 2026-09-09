// pnl_profiler.cpp                                                   -*-C++-*-
#include <ui/pnl/pnl_profiler.h>

// std
#include <algorithm>

// imgui
#include <imgui.h>

namespace eng::ui {

namespace {

/// Interval, in seconds, between two refreshes of the textual figures.
constexpr float k_REFRESH_PERIOD = 0.1f;

/// Height, in pixels, of the frame time plot.
constexpr float k_PLOT_HEIGHT = 80.0f;

/// Indentation, in pixels, of one nesting level in the breakdown table.
constexpr float k_INDENT = 12.0f;

/// Colour of the figures exceeding the frame budget.
const ImVec4 k_OVER_BUDGET_COLOR = ImVec4(0.90f, 0.35f, 0.35f, 1.0f);

/// Colour of the figures within the frame budget.
const ImVec4 k_IN_BUDGET_COLOR = ImVec4(0.55f, 0.85f, 0.55f, 1.0f);

/// Flags shared by the tables of this panel.
constexpr ImGuiTableFlags k_TABLE_FLAGS = ImGuiTableFlags_Borders |
                                          ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_SizingStretchProp;

/// Return the frame duration of the record at the specified 'idx' of the
/// profiler pointed to by the specified 'data'. This is the accessor handed
/// to 'ImGui::PlotLines' so that the ring buffer needs no flattening.
float frameTimeGetter(void* data, const int idx)
{
    const auto* profiler = static_cast<const core::Profiler*>(data);
    return profiler->frameAt(static_cast<uint32_t>(idx)).totalMs;
}

/// Display the specified 'label' followed by the specified 'ms' duration,
/// coloured according to the specified 'budgetMs'.
void textMs(const char* label, const float ms, const float budgetMs)
{
    ImGui::TextColored(ms > budgetMs ? k_OVER_BUDGET_COLOR : k_IN_BUDGET_COLOR,
                       "%s %6.2f ms",
                       label,
                       static_cast<double>(ms));
}

}  // close unnamed namespace

ProfilerPanel::ProfilerPanel(core::Profiler* profiler)
: d_profiler_p(profiler)
{
}

void ProfilerPanel::render()
{
    if (d_profiler_p == nullptr) {
        return;
    }

    core::Profiler& profiler = *d_profiler_p;

    ImGui::Begin("Profiler");

    if (profiler.historySize() == 0) {
        ImGui::TextUnformatted("Waiting for the first frame...");
        ImGui::End();
        return;
    }

    // Refresh the snapshot a few times per second only: figures updated at
    // the frame rate are unreadable.
    d_refreshAccumulator += ImGui::GetIO().DeltaTime;

    if (d_refreshAccumulator >= k_REFRESH_PERIOD ||
        d_snapshot.totalMs == 0.0f) {
        d_refreshAccumulator = 0.0f;
        d_snapshot           = profiler.lastFrame();
        d_frameStats         = profiler.frameStats();
    }

    // -------------------------------------------------------------------
    // Frame timing
    // -------------------------------------------------------------------

    const float fps = d_snapshot.totalMs > 0.0f ? 1000.0f / d_snapshot.totalMs
                                                : 0.0f;

    textMs("Frame", d_snapshot.totalMs, d_budgetMs);
    ImGui::SameLine();
    ImGui::Text("(%.0f FPS)", static_cast<double>(fps));

    ImGui::Text("avg %5.2f   min %5.2f   max %5.2f   1%% low %5.2f  [ms]",
                static_cast<double>(d_frameStats.avgMs),
                static_cast<double>(d_frameStats.minMs),
                static_cast<double>(d_frameStats.maxMs),
                static_cast<double>(d_frameStats.p99Ms));

    // A fixed scale is the default on purpose: an auto-scaled plot
    // renormalizes on every spike, which hides the very spikes it should
    // expose.
    const float scaleMax = d_fixedScale
                               ? d_budgetMs * 2.0f
                               : std::max(d_frameStats.maxMs, 1.0f) * 1.1f;

    ImGui::PlotLines("##frametime",
                     &frameTimeGetter,
                     &profiler,
                     static_cast<int>(profiler.historySize()),
                     0,
                     nullptr,
                     0.0f,
                     scaleMax,
                     ImVec2(-1.0f, k_PLOT_HEIGHT));

    bool paused = profiler.isPaused();
    if (ImGui::Checkbox("Pause", &paused)) {
        profiler.setPaused(paused);
    }

    ImGui::SameLine();
    ImGui::Checkbox("Fixed scale", &d_fixedScale);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::DragFloat("Budget (ms)", &d_budgetMs, 0.1f, 1.0f, 100.0f, "%.2f");

    // -------------------------------------------------------------------
    // Per-scope breakdown
    // -------------------------------------------------------------------

    if (ImGui::CollapsingHeader("Breakdown", ImGuiTreeNodeFlags_DefaultOpen) &&
        ImGui::BeginTable("scopes", 5, k_TABLE_FLAGS)) {
        ImGui::TableSetupColumn("Scope");
        ImGui::TableSetupColumn("CPU ms");
        ImGui::TableSetupColumn("%");
        ImGui::TableSetupColumn("Calls");
        ImGui::TableSetupColumn("GPU ms");
        ImGui::TableHeadersRow();

        float accounted = 0.0f;

        for (uint16_t id = 0; id < profiler.scopeCount(); ++id) {
            const float   cpuMs  = d_snapshot.cpuMs[id];
            const uint8_t depth  = profiler.scopeDepth(id);
            const float   indent = depth * k_INDENT;

            // Only the top level scopes partition the frame; a nested one is
            // already counted inside its parent.
            if (depth == 0) {
                accounted += cpuMs;
            }

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            if (depth > 0) {
                ImGui::Indent(indent);
            }
            ImGui::TextUnformatted(profiler.scopeName(id));
            if (depth > 0) {
                ImGui::Unindent(indent);
            }

            ImGui::TableNextColumn();
            ImGui::Text("%6.3f", static_cast<double>(cpuMs));

            ImGui::TableNextColumn();
            const float share = d_snapshot.totalMs > 0.0f
                                    ? 100.0f * cpuMs / d_snapshot.totalMs
                                    : 0.0f;
            ImGui::Text("%5.1f", static_cast<double>(share));

            ImGui::TableNextColumn();
            ImGui::Text("%u", static_cast<unsigned>(d_snapshot.hits[id]));

            ImGui::TableNextColumn();
            const float gpuMs = d_snapshot.gpuMs[id];
            if (gpuMs > 0.0f) {
                ImGui::Text("%6.3f", static_cast<double>(gpuMs));
            }
            else {
                ImGui::TextUnformatted("-");
            }
        }

        // Whatever the instrumented scopes did not account for: vsync,
        // driver work, and any region left uninstrumented.
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("Unaccounted");
        ImGui::TableNextColumn();
        ImGui::TextDisabled(
            "%6.3f",
            static_cast<double>(d_snapshot.totalMs - accounted));
        ImGui::TableNextColumn();
        ImGui::TextDisabled("-");
        ImGui::TableNextColumn();
        ImGui::TextDisabled("-");
        ImGui::TableNextColumn();
        ImGui::TextDisabled("-");

        ImGui::EndTable();
    }

    // -------------------------------------------------------------------
    // Counters
    // -------------------------------------------------------------------

    if (profiler.counterCount() > 0 &&
        ImGui::CollapsingHeader("Counters", ImGuiTreeNodeFlags_DefaultOpen) &&
        ImGui::BeginTable("counters", 2, k_TABLE_FLAGS)) {
        ImGui::TableSetupColumn("Counter");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        for (uint16_t id = 0; id < profiler.counterCount(); ++id) {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(profiler.counterName(id));

            ImGui::TableNextColumn();
            ImGui::Text("%lld",
                        static_cast<long long>(d_snapshot.counters[id]));
        }

        ImGui::EndTable();
    }

    ImGui::Text("Frame #%llu   history %u",
                static_cast<unsigned long long>(d_snapshot.frameIndex),
                profiler.historySize());

    ImGui::End();
}

}  // close package namespace
