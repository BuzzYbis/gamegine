// mesh_component.h                                                   -*-C++-*-
#ifndef INCLUDED_ENGINE_SCENE_COMPONENTS_MESH_COMPONENT_H
#define INCLUDED_ENGINE_SCENE_COMPONENTS_MESH_COMPONENT_H

//@PURPOSE: Provide a component linking an entity to its 3D geometry and
// material.
//
//@CLASSES:
//  engine::scene::MeshComponent: ECS component for rendering 3D meshes.
//
//@DESCRIPTION: This component stores references to the graphical assets
// required to draw an entity. It holds shared ownership of a `renderer::Mesh`
// (managed by the `AssetManager`) to ensure the GPU buffers remain valid as
// long as the entity exists.

// renderer
#include <renderer/material.h>
#include <renderer/mesh.h>

// std
#include <memory>

namespace engine::scene {

// --------------------
// struct MeshComponent
// --------------------

struct MeshComponent {
    // DATA

    /// Shared ownership of the 3D geometries (Vertex/Index buffers).
    std::vector<std::shared_ptr<renderer::Mesh> > d_meshes;

    /// Non-owning pointer to the material used to shade the mesh.
    /// (Note: This could also become a shared_ptr later if materials
    /// are managed by the AssetManager).
    std::vector<renderer::Material*> d_materials;
};

}  // close engine::scene namespace

#endif  // INCLUDED_ENGINE_SCENE_COMPONENTS_MESH_COMPONENT_H