// engine/scene/scene.cpp
#include "scene.h"
#include "components/camera_component.h"
#include "components/mesh_component.h"
#include "components/tag_component.h"
#include "components/transform_component.h"
#include "core/input.h"
#include "entity.h"

#include <glm/gtc/constants.hpp>

namespace engine::scene {

Entity Scene::createEntity(const std::string& name)
{
    // Generate the unique id into the registry (database)
    const entt::entity handle = d_registry.create();

    // Create a wrapper so that we can manipulate the object through Entity
    Entity entity(handle, this);

    // Add mandatory components. Each object must have a position and a name
    entity.addComponent<TransformComponent>();
    auto& [d_name] = entity.addComponent<TagComponent>();
    d_name         = name.empty() ? "Entity" : name;

    return entity;
}

void Scene::destroyEntity(const Entity entity)
{
    d_registry.destroy(entity.entityHandle());
}

}  // close engine::scene namespace