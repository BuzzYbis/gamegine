// rhi_types.h                                                        -*-C++-*-
#ifndef INCLUDED_ENG_RHI_TYPES_H
#define INCLUDED_ENG_RHI_TYPES_H

//@PURPOSE: Provide common types and enumerations for the RHI.
//
//@CLASSES:
//  eng::rhi::GraphicsAPI: Enumeration of supported graphics backends.
//  eng::rhi::Format: Enumeration of pixel and vertex formats.
//  eng::rhi::VertexBindingDescriptor: Memory layout for vertex buffers.
//  eng::rhi::MeshPushConstants: Per-object data sent to shaders.
//
//@DESCRIPTION: This component provides a centralized set of technical types,
// enumerations, and simple structures used across the Render Hardware
// Interface (RHI). These types ensure consistency between the high-level
// renderer and the various API-specific backends.

// std
#include <vector>

// third-party
#include <glm/glm.hpp>

namespace eng::rhi {

/// Enumerates the supported graphics APIs for the RHI and UI backends.
enum class GraphicsAPI {
    Vulkan,  // Vulkan backend (cross-platform).
    // Metal    // Apple Metal backend (macOS/iOS).
};

enum class Format {
    Undefined,
    R8G8B8A8_UNorm,
    R8G8B8A8_SRGB,
    B8G8R8A8_SRGB,
    D32_SFloat
};

enum class LoadOp { Load, Clear, DontCare };

enum class StoreOp { Store, DontCare };

enum class CullMode { Front, Back, None };

enum class PolygonMode { Fill, Line, Point };

struct ClearColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

struct Extent2D {
    uint32_t width  = 0;
    uint32_t height = 0;
};

// Data types that can be sent to the Vertex Shader
enum class VertexFormat {
    Float1,  // float
    Float2,  // vec2
    Float3,  // vec3
    Float4,  // vec4
    Int1,
    Int2,
    Int3,
    Int4,
};

// Controls whether we advance in the buffer for each vertex or each instance.
enum class VertexInputRate { Vertex, Instance };

// Describes a full Buffer (e.g., "The buffer at slot 0 contains 32-byte
// elements")
struct VertexBindingDescriptor {
    uint32_t        binding   = 0;
    uint32_t        stride    = 0;
    VertexInputRate inputRate = VertexInputRate::Vertex;
};

// Describes a specific attribute in the Buffer (e.g., "Position is a Float3
// starting at byte 0")
struct VertexAttributeDescriptor {
    uint32_t     location = 0;  // The layout(location = X) in the shader
    uint32_t     binding  = 0;  // The buffer slot it comes from
    VertexFormat format   = VertexFormat::Float3;
    uint32_t     offset   = 0;  // The offset in bytes
};

enum class ResourceType {
    UniformBuffer,
    TextureSampler,
};

enum class ShaderStage {
    Vertex,
    Fragment,
};

struct ResourceBinding {
    uint32_t     binding;
    ResourceType type;
    ShaderStage  stage;
};

struct ResourceLayoutConfig {
    std::vector<ResourceBinding> bindings;
};

struct MeshPushConstants {
    glm::mat4 renderMatrix;  // Model matrix (offset 0 for shader)
};

}  // close package namespace
#endif  // INCLUDED_ENG_RHI_TYPES_H