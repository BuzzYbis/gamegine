// comp_mesh.h                                                        -*-C++-*-
#ifndef INCLUDED_SCN_COMP_MESH_H
#define INCLUDED_SCN_COMP_MESH_H

//@PURPOSE: Provide a component linking an entity to its 3D geometry and
// material.
//
//@CLASSES:
//  eng::scn::comp::MeshComponent: ECS component for rendering 3D meshes.
//
//@DESCRIPTION: This component stores the geometry an entity is drawn with,
// as the batches produced by the asset manager from an imported model. Each
// batch holds shared ownership of its 'rnd::Mesh', so the GPU buffers stay
// valid as long as the entity does, and carries the placements that mesh
// occupies inside the model. The transform of the entity applies on top of
// those placements.

// std
#include <vector>

// renderer
#include <rnd/meshbatch.h>

namespace eng::scn::comp {

// ====================
// struct MeshComponent
// ====================

/// This structure holds the geometry an entity is drawn with.
struct MeshComponent {
    // DATA

    /// The geometry of this entity, one batch per distinct mesh.
    std::vector<rnd::MeshBatch> batches;
};

}  // close package namespace
#endif  // INCLUDED_SCN_COMP_MESH_H
