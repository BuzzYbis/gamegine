// rhi_pipelineprotocol.h                                             -*-C++-*-
#ifndef INCLUDED_ENG_RHI_PIPELINEPROTOCOL_H
#define INCLUDED_ENG_RHI_PIPELINEPROTOCOL_H

//@PURPOSE: Provide a pure abstract interface for a graphics pipeline.
//
//@CLASSES:
//  eng::rhi::PipelineConfig: Configuration parameters for pipeline creation.
//  eng::rhi::PipelineProtocol: Protocol for GPU pipeline state objects.
//
//@DESCRIPTION: This component provides a pure abstract interface,
// 'eng::rhi::PipelineProtocol', that represents a compiled graphics pipeline
// on the GPU. It also provides an 'eng::rhi::PipelineConfig' structure used
// to pass all necessary state (shaders, rasterization, depth, blending) to
// the API-specific backend when creating a new pipeline.

// std
#include <string>
#include <vector>

// rhi
#include <rhi/rhi_types.h>

namespace eng::rhi {

// Forward declarations
class ResourceLayoutProtocol;

// =====================
// struct PipelineConfig
// =====================

/// This struct holds all the configuration state required to compile a
/// graphics pipeline. It acts as a platform-agnostic descriptor.
struct PipelineConfig {
    // DATA

    /// Name of the vertex shader (generic, without extension).
    std::string vertexShaderName;

    /// Name of the fragment shader (generic, without extension).
    std::string fragmentShaderName;

    /// Memory layout of vertex buffers.
    std::vector<VertexBindingDescriptor> vertexBindings;

    /// Mapping of vertex data to shader inputs.
    std::vector<VertexAttributeDescriptor> vertexAttributes;

    /// Pixel format of the target render surface.
    Format colorAttachmentFormat = Format::B8G8R8A8_SRGB;

    /// Format of the depth buffer.
    Format depthAttachmentFormat = Format::D32_SFloat;

    /// Fill, wireframe, or point rendering.
    PolygonMode polygonMode = PolygonMode::Fill;

    /// Face culling mode (e.g., Back, Front, None).
    CullMode cullMode = CullMode::Front;

    /// Enable/disable depth comparison.
    bool enableDepthTest = true;

    /// Enable/disable writing to the depth buffer.
    bool enableDepthWrite = true;

    /// Enable/disable alpha color blending.
    bool enableBlending = false;

    /// Descriptor set layouts (uniforms, textures).
    std::vector<ResourceLayoutProtocol*> resourceLayouts;

    /// Size of push constants in bytes.
    uint32_t pushConstantSize = 0;

    /// Shader stages that can access push constants.
    ShaderStage pushConstantStage = ShaderStage::Vertex;
};

// ======================
// class PipelineProtocol
// ======================

/// This class provides a protocol (pure abstract interface) representing a
/// compiled graphics pipeline on the GPU.
class PipelineProtocol {
  public:
    // CREATORS

    /// Destroy this pipeline and release its associated GPU resources.
    virtual ~PipelineProtocol() = default;
};

}  // close package namespace
#endif  // INCLUDED_ENG_RHI_PIPELINEPROTOCOL_H