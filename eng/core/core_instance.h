// core_instance.h                                                    -*-C++-*-
#ifndef INCLUDED_ENG_CORE_INSTANCE_H
#define INCLUDED_ENG_CORE_INSTANCE_H

//@PURPOSE: Provide a standard structure for per-instance vertex data.
//
//@CLASSES:
//  eng::core::Instance: POD structure representing one placement of a mesh.
//
//@DESCRIPTION: This component defines the 'eng::core::Instance' structure,
// which carries the placement a mesh is drawn at when several placements of
// that mesh are drawn by a single instanced call. Where 'eng::core::Vertex'
// describes data the pipeline steps through once per vertex, this structure
// describes data it steps through once per instance, read from a vertex
// buffer of its own bound at 'k_BINDING'.

// std
#include <cstddef>
#include <cstdint>

// core
#include <core/core_vertex.h>

// rhi
#include <rhi/rhi_pipelineprotocol.h>
#include <rhi/rhi_types.h>

// third-party
#include <glm/glm.hpp>

namespace eng::core {

// ===============
// struct Instance
// ===============

/// This structure represents the data attached to a single placement of a
/// mesh inside an instanced draw call.
struct Instance {
    // CLASS DATA

    /// The vertex buffer slot the instance data is bound to, the one past
    /// the slot 'Vertex' occupies.
    static constexpr uint32_t k_BINDING = Vertex::k_BINDING + 1;

    /// The first of the locations the placement occupies, following the
    /// last location 'Vertex' claims.
    static constexpr uint32_t k_FIRST_LOCATION = Vertex::k_LOCATION_COUNT;

    /// The number of attributes the placement is split across, one per
    /// column of the matrix.
    static constexpr uint32_t k_COLUMN_COUNT = 4;

    // DATA

    /// The placement of the mesh, expressed in the space of the model it
    /// belongs to. Reaches the shader as 'k_COLUMN_COUNT' attributes, one
    /// per column.
    glm::mat4 model = glm::mat4(1.0f);

    // CLASS METHODS

    /// Configure the specified 'config' with the memory layout descriptors
    /// corresponding to this instance structure. The behavior is undefined
    /// unless 'config' has not already been given a binding numbered
    /// 'k_BINDING' or an attribute among the locations this structure
    /// claims.
    static void populatePipelineConfig(rhi::PipelineConfig& config)
    {
        config.vertexBindings.push_back(
            {k_BINDING, sizeof(Instance), rhi::VertexInputRate::Instance});

        // The pipeline knows nothing of matrices, so the placement is
        // declared as the columns it is made of, each at its own offset.
        for (uint32_t column = 0; column < k_COLUMN_COUNT; ++column) {
            config.vertexAttributes.push_back(
                {k_FIRST_LOCATION + column,
                 k_BINDING,
                 rhi::VertexFormat::Float4,
                 static_cast<uint32_t>(offsetof(Instance, model) +
                                       column * sizeof(glm::vec4))});
        }
    }
};

}  // close package namespace

#endif  // INCLUDED_ENG_CORE_INSTANCE_H
