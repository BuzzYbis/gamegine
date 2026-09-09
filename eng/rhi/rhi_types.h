// rhi_types.h                                                        -*-C++-*-
#ifndef INCLUDED_ENG_RHI_TYPES_H
#define INCLUDED_ENG_RHI_TYPES_H

//@PURPOSE: Provide common types and enumerations for the RHI.
//
//@CLASSES:
//  eng::rhi::GraphicsAPI: Enumeration of supported graphics backends.
//  eng::rhi::Format: Enumeration of pixel and vertex formats.
//  eng::rhi::VertexBindingDescriptor: Memory layout for vertex buffers.
//  eng::rhi::DeviceCapabilities: Optional GPU features reported by a device.
//  eng::rhi::MeshPushConstants: Per-object data sent to shaders.
//
//@DESCRIPTION: This component provides a centralized set of technical types,
// enumerations, and simple structures used across the Render Hardware
// Interface (RHI). These types ensure consistency between the high-level
// renderer and the various API-specific backends.

// std
#include <cstdint>
#include <string>
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
    VertexFragment,
};

struct ResourceBinding {
    uint32_t     binding;
    ResourceType type;
    ShaderStage  stage;
};

struct ResourceLayoutConfig {
    std::vector<ResourceBinding> bindings;
};

// ========================
// struct DeviceCapabilities
// ========================

/// This struct reports the optional GPU capabilities that higher layers may
/// branch on when selecting a rendering path. It is populated once during
/// 'ContextProtocol::initialize' and is immutable thereafter. It deliberately
/// holds no backend-specific types, so a renderer can pick a path without
/// including any graphics API header.
struct DeviceCapabilities {
    // DATA

    // -- Identity

    /// Human-readable name of the selected physical device.
    std::string deviceName;

    /// Human-readable driver name and version.
    std::string driverInfo;

    /// Major component of the supported API version.
    uint32_t apiVersionMajor = 0;

    /// Minor component of the supported API version.
    uint32_t apiVersionMinor = 0;

    // -- Bindless resources and GPU-driven submission

    /// 'true' if shaders can dereference raw device pointers into buffers.
    bool bufferDeviceAddress = false;

    /// 'true' if descriptors can be indexed by a shader-computed value.
    bool descriptorIndexing = false;

    /// 'true' if descriptor arrays may be declared without a fixed size.
    bool runtimeDescriptorArray = false;

    /// 'true' if sampled image arrays accept non-uniform indices.
    bool nonUniformImageIndexing = false;

    /// 'true' if a descriptor binding may be left partially populated.
    bool partiallyBoundDescriptors = false;

    /// 'true' if many draws may be sourced from a single buffer.
    bool multiDrawIndirect = false;

    /// 'true' if the draw count itself may be read from a GPU buffer. When
    /// this is 'false' but 'multiDrawIndirect' is 'true', a GPU-driven
    /// pipeline must dispatch a fixed maximum count and zero the instance
    /// count of culled entries instead of removing them.
    bool drawIndirectCount = false;

    /// 'true' if draw parameters such as 'gl_DrawID' are visible to shaders.
    bool shaderDrawParameters = false;

    // -- 64-bit integer support

    /// 'true' if shaders may perform 64-bit integer arithmetic.
    bool shaderInt64 = false;

    /// 'true' if 64-bit atomic operations on buffers are supported.
    bool bufferInt64Atomics = false;

    /// 'true' if 64-bit atomic operations on images are supported. A
    /// software rasterizer needs this to resolve depth and primitive
    /// identity in a single atomic maximum.
    bool imageInt64Atomics = false;

    // -- Optional geometry pipeline

    /// 'true' if the mesh shader stage is available.
    bool meshShader = false;

    /// 'true' if the task (amplification) shader stage is available.
    bool taskShader = false;

    // -- Properties

    /// Number of invocations that execute in lockstep, or 0 if unknown.
    uint32_t subgroupSize = 0;

    /// Smallest subgroup size the device can be asked to use.
    uint32_t minSubgroupSize = 0;

    /// Largest subgroup size the device can be asked to use.
    uint32_t maxSubgroupSize = 0;

    /// Maximum number of invocations in a single compute work group.
    uint32_t maxComputeWorkGroupInvocations = 0;

    /// Maximum number of sampled images addressable from one shader stage
    /// through update-after-bind descriptors. This is the practical ceiling
    /// on the size of a bindless material table.
    uint32_t maxBindlessSampledImages = 0;

    // ACCESSORS

    /// Return 'true' if this device can drive draw submission from the GPU
    /// without rebinding descriptors per object.
    [[nodiscard]]
    bool supportsGpuDrivenSubmission() const;

    /// Return 'true' if this device can run the 64-bit variant of a software
    /// rasterizer, which resolves depth and identity in one atomic maximum.
    [[nodiscard]]
    bool supports64BitVisibilityAtomics() const;
};

struct MeshPushConstants {
    glm::mat4 renderMatrix;  // Model matrix (offset 0 for shader)
};

// =======================================================================
//                          INLINE DEFINITIONS
// =======================================================================

// ------------------------
// struct DeviceCapabilities
// ------------------------

// ACCESSORS

inline bool DeviceCapabilities::supportsGpuDrivenSubmission() const
{
    return bufferDeviceAddress && descriptorIndexing &&
           runtimeDescriptorArray &&
           (drawIndirectCount || multiDrawIndirect);
}

inline bool DeviceCapabilities::supports64BitVisibilityAtomics() const
{
    return shaderInt64 && imageInt64Atomics;
}

}  // close package namespace
#endif  // INCLUDED_ENG_RHI_TYPES_H