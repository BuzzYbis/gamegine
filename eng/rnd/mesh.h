// mesh.h                                                             -*-C++-*-
#ifndef INCLUDED_ENG_RND_MESH_H
#define INCLUDED_ENG_RND_MESH_H

//@PURPOSE: Provide a high-level representation of a 3D geometric mesh.
//
//@CLASSES:
//  eng::rnd::Mesh: Encapsulates vertex and index buffers for rendering.
//
//@DESCRIPTION: This component provides the 'eng::rnd::Mesh' class, which
// manages the GPU resources (vertex and index buffers) required to represent
// a 3D object. It abstracts the low-level RHI buffer management and provides
// a simple 'draw' method for the renderer.

// std
#include <memory>
#include <vector>

// core
#include <core/core_vertex.h>

// rhi
#include <rhi/rhi_bufferprotocol.h>
#include <rhi/rhi_commandlistprotocol.h>
#include <rhi/rhi_contextprotocol.h>

namespace eng::rnd {

// ==========
// class Mesh
// ==========

/// This class encapsulates the GPU memory and draw logic for 3D geometry.
class Mesh {
  private:
    // DATA
    std::unique_ptr<rhi::BufferProtocol> m_vertexBuffer;
    std::unique_ptr<rhi::BufferProtocol> m_indexBuffer;

    uint32_t m_indexCount = 0;

  public:
    // CREATORS

    /// Create a mesh using the specified 'context', 'vertices', and 'indices'.
    /// The data is immediately uploaded to the GPU.
    Mesh(rhi::ContextProtocol*            context,
         const std::vector<core::Vertex>& vertices,
         const std::vector<uint32_t>&     indices);

    /// Destroy this mesh and release its associated GPU buffers.
    ~Mesh() = default;

    // ACCESSORS

    /// Return the number of indices in this mesh.
    [[nodiscard]]
    uint32_t getIndexCount() const
    {
        return m_indexCount;
    }

    // MANIPULATORS

    /// Record the commands necessary to bind and draw this mesh into the
    /// specified 'cmd' command list.
    void draw(rhi::CommandListProtocol* cmd) const;
};

}  // close package namespace

#endif  // INCLUDED_ENG_RND_MESH_H