// entities_panel.cpp                                                 -*-C++-*-
#include <ui/panels/entities_panel.h>

// imgui
#include <imgui.h>

// scene
#include <scene/components/tag_component.h>
#include <scene/components/transform_component.h>
#include <scene/scene.h>

namespace engine::ui {
EntitiesPanel::EntitiesPanel(scene::Scene* scene)
: d_scene_p(scene)
{
}

void EntitiesPanel::render()
{
    if (d_scene_p == nullptr) {
        return;
    }

    ImGui::Begin("Scene");

    const auto view =
        d_scene_p->registry()
            .view<scene::TagComponent, scene::TransformComponent>();

    for (const auto entity : view) {
        auto [d_name] = view.get<scene::TagComponent>(entity);
        ImGui::Text("%s", d_name.c_str());
    }

    ImGui::End();
}

}  // close engine::ui namespace