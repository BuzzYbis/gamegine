// camera_system.h                                                  -*-C++-*-
#ifndef INCLUDED_ENGINE_SCENE_SYSTEMS_CAMERA_SYSTEM_H
#define INCLUDED_ENGINE_SCENE_SYSTEMS_CAMERA_SYSTEM_H

// scene
#include <scene/systems/isystem.h>

namespace engine::scene {

class CameraSystem : public ISystem {
  public:
    void update(entt::registry&     registry,
                core::InputManager& inputManager,
                float               dt) override;
};

}  // close engine::scene namespace

#endif  // INCLUDED_ENGINE_SCENE_SYSTEMS_CAMERA_SYSTEM_H
