// pnl_entities.cpp                                                   -*-C++-*-
#include <ui/pnl/pnl_entities.h>

// scene
#include <scn/comp/comp_tag.h>
#include <scn/comp/comp_transform.h>
#include <scn/scn_scene.h>

// third-pary
#include <imgui.h>

namespace eng::ui::pnl {

// -------------------
// class EntitiesPanel
// -------------------

// CREATORS
EntitiesPanel::EntitiesPanel(scn::Scene* scene)
: d_scene_p(scene)
{
}

// MANIPULATORS
void EntitiesPanel::render()
{
    if (d_scene_p == nullptr) {
        return;
    }

    ImGui::Begin("Scene");

    const auto view =
        d_scene_p->registry()
            .view<scn::comp::TagComponent, scn::comp::TransformComponent>();

    for (const auto entity : view) {
        auto [d_name] = view.get<scn::comp::TagComponent>(entity);
        ImGui::Text("%s", d_name.c_str());
    }

    ImGui::End();
}

}  // close package namespace