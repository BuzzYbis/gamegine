// meshbatch.h                                                        -*-C++-*-
#ifndef INCLUDED_ENG_RND_MESHBATCH_H
#define INCLUDED_ENG_RND_MESHBATCH_H

//@PURPOSE: Provide a mesh paired with the placements it is drawn at.
//
//@CLASSES:
//  eng::rnd::MeshBatch: A mesh, its material, and the placements it is at.
//
//@DESCRIPTION: This component provides the 'eng::rnd::MeshBatch' structure,
// which gathers under a single mesh every placement that mesh occupies in a
// model. A file naming the same geometry from several of its nodes yields
// one batch holding several transforms, so that the geometry is uploaded
// once and the whole batch is drawn by a single instanced call.
//
// The placements are held twice: as the matrices 'instances' the CPU can
// still read, and as the vertex buffer 'instanceBuffer' the pipeline steps
// through once per instance, laid out as 'eng::core::Instance' describes.
// They are filled together and neither is written again, a placement being
// fixed in the space of the model it was imported from. What varies from
// one frame to the next is the transform of the entity the batch is drawn
// under, which reaches the shader as a push constant instead.
//
// Keeping the matrices past the upload costs the memory of a second copy,
// which drawing alone would not repay: the renderer reads their count and
// nothing else. They are kept for the work that has to consult a placement
// without stalling on the device, culling before anything else.
//
// Two batches naming the same mesh share its geometry, the mesh being held
// by a shared pointer, but not their placements: batches built from two
// separate calls hold a device buffer each even where the placements agree.

// std
#include <memory>
#include <vector>

// core
#include <core/core_instance.h>

// renderer
#include <rnd/material.h>
#include <rnd/mesh.h>

// rhi
#include <rhi/rhi_bufferprotocol.h>

// third-party
#include <glm/glm.hpp>

namespace eng::rnd {

// ================
// struct MeshBatch
// ================

/// This structure pairs a mesh with the transforms it is to be drawn at.
struct MeshBatch {
    // DATA

    /// Shared ownership of the geometry (vertex and index buffers).
    std::shared_ptr<Mesh> mesh;

    /// Non-owning pointer to the material shading every placement.
    Material* material = nullptr;

    /// The placements of 'mesh' inside the model it was imported from,
    /// expressed in the space of that model. Never empty: a mesh no
    /// placement names yields no batch at all.
    std::vector<glm::mat4> instances;

    /// Shared ownership of the vertex buffer holding 'instances' on the
    /// device, bound at 'core::Instance::k_BINDING' and stepped through
    /// once per instance. Holds exactly 'instances.size()' placements, in
    /// the order 'instances' lists them.
    std::shared_ptr<rhi::BufferProtocol> instanceBuffer;
};

}  // close package namespace
#endif  // INCLUDED_ENG_RND_MESHBATCH_H
