// scene.cpp                                                          -*-C++-*-
#include <scn/scn_scene.h>

// scn
#include <scn/comp/comp_tag.h>
#include <scn/comp/comp_transform.h>
#include <scn/scn_entity.h>

namespace eng::scn {

Entity Scene::createEntity(const std::string& name)
{
    // Generate the unique id into the registry (database).
    const entt::entity handle = d_registry.create();

    // Create a wrapper so that we can manipulate the object through Entity
    Entity entity(handle, this);

    // Add mandatory components. Each object must have a position and a name.
    entity.addComponent<comp::TransformComponent>(
        glm::vec3(0.0f, 0.0f, 0.0f));
    auto& [d_name] = entity.addComponent<comp::TagComponent>();
    d_name         = name.empty() ? "Entity" : name;

    return entity;
}

void Scene::destroyEntity(const Entity entity)
{
    d_registry.destroy(entity.entityHandle());
}

}  // close package namespace