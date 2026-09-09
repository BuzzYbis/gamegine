// core_vertex.h                                                      -*-C++-*-
#ifndef INCLUDED_ENG_CORE_VERTEX_H
#define INCLUDED_ENG_CORE_VERTEX_H

//@PURPOSE: Provide a standard structure for 3D vertex data.
//
//@CLASSES:
//  eng::core::Vertex: POD structure representing a single 3D point.
//
//@DESCRIPTION: This component defines the standard 'eng::core::Vertex'
// structure used throughout the engine for geometry processing. It contains
// 3D position, normal, and UV coordinates. It also provides a utility method
// to automatically populate RHI pipeline configurations with the correct
// vertex memory layout.
//
// The buffer slot this structure is bound to, and the number of attribute
// locations it claims, are published as 'k_BINDING' and 'k_LOCATION_COUNT'.
// Per-instance data laid alongside it, 'eng::core::Instance' among it, has
// to begin past both, and reads them rather than restating the numbers it
// sees today. An attribute added here therefore moves what follows instead
// of quietly landing on top of it.

// std
#include <cstddef>

// rhi
#include <rhi/rhi_pipelineprotocol.h>
#include <rhi/rhi_types.h>

// third-party
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

namespace eng::core {

// ============
// struct Vertex
// ============

/// This structure represents the data for a single vertex in a 3D mesh.
struct Vertex {
    // CLASS DATA

    /// The vertex buffer slot this structure is bound to.
    static constexpr uint32_t k_BINDING = 0;

    /// The number of attribute locations this structure claims, counted
    /// from zero. Data of another rate begins past it.
    static constexpr uint32_t k_LOCATION_COUNT = 3;

    // DATA
    glm::vec3 position;  // 3D coordinates.
    glm::vec3 normal;    // Surface normal vector.
    glm::vec2 uv;        // Texture coordinates.

    // CLASS METHODS

    /// Configure the specified 'config' with the memory layout descriptors
    /// corresponding to this vertex structure.
    static void populatePipelineConfig(rhi::PipelineConfig& config)
    {
        config.vertexBindings.push_back(
            {k_BINDING, sizeof(Vertex), rhi::VertexInputRate::Vertex});

        config.vertexAttributes.push_back({0,
                                           k_BINDING,
                                           rhi::VertexFormat::Float3,
                                           offsetof(Vertex, position)});

        config.vertexAttributes.push_back({1,
                                           k_BINDING,
                                           rhi::VertexFormat::Float3,
                                           offsetof(Vertex, normal)});

        config.vertexAttributes.push_back({2,
                                           k_BINDING,
                                           rhi::VertexFormat::Float2,
                                           offsetof(Vertex, uv)});
    }

    // ACCESSORS

    /// Return 'true' if this vertex is identical to the specified 'other'
    /// vertex, and 'false' otherwise.
    bool operator==(const Vertex& other) const
    {
        return position == other.position && normal == other.normal &&
               uv == other.uv;
    }
};

}  // close package namespace

// std::hash injection
template <>
struct std::hash<eng::core::Vertex> {
    size_t operator()(eng::core::Vertex const& vertex) const noexcept
    {
        return (std::hash<glm::vec3>()(vertex.position) ^
                std::hash<glm::vec3>()(vertex.normal) << 1) >>
                   1 ^
               std::hash<glm::vec2>()(vertex.uv) << 1;
    }
};

#endif  // INCLUDED_ENG_CORE_VERTEX_H