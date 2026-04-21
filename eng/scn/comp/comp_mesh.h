// comp_mesg.h                                                        -*-C++-*-
#ifndef INCLUDED_SCN_COMP_MESH_H
#define INCLUDED_SCN_COMP_MESH_H

//@PURPOSE: Provide a component linking an entity to its 3D geometry and
// material.
//
//@CLASSES:
//  eng::scn::comp::MeshComponent: ECS component for rendering 3D meshes.
//
//@DESCRIPTION: This component stores references to the graphical assets
// required to draw an entity. It holds shared ownership of a `rnd::Mesh`
// (managed by the `AssetManager`) to ensure the GPU buffers remain valid as
// long as the entity exists.

// renderer
#include <rnd/material.h>
#include <rnd/mesh.h>

// std
#include <memory>

namespace eng::scn::comp {

// ====================
// struct MeshComponent
// ====================

struct MeshComponent {
    // DATA

    /// Shared ownership of the 3D geometries (Vertex/Index buffers).
    std::vector<std::shared_ptr<rnd::Mesh> > d_meshes;

    /// Non-owning pointer to the material used to shade the mesh.
    std::vector<rnd::Material*> d_materials;
};

}  // close package namespace
#endif  // INCLUDED_SCN_COMP_MESH_H