// tag_component.h                                                    -*-C++-*-
#ifndef INCLUDED_ENGINE_SCENE_COMPONENTS_TAG_COMPONENT_H
#define INCLUDED_ENGINE_SCENE_COMPONENTS_TAG_COMPONENT_H

// std
#include <string>

namespace engine::scene {

struct TagComponent {
    std::string d_name = "Entity";
};

}  // close engine::scene

#endif  // INCLUDED_ENGINE_SCENE_COMPONENTS_TAG_COMPONENT_H