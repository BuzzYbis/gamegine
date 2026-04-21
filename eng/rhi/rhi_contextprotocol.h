// rhi_contextprotocol.h                                              -*-C++-*-
#ifndef INCLUDED_ENG_RHI_CONTEXTPROTOCOL_H
#define INCLUDED_ENG_RHI_CONTEXTPROTOCOL_H

//@PURPOSE: Provide a pure abstract interface for the RHI context.
//
//@CLASSES:
//  eng::rhi::ContextProtocol: Protocol for RHI state and resource factory.
//
//@DESCRIPTION: This component provides a pure abstract interface,
// 'eng::rhi::ContextProtocol', that serves as the main entry point for the
// Render Hardware Interface (RHI). It acts as a central factory for allocating
// GPU resources (swapchains, buffers, pipelines) and managing the underlying
// graphics API state.

// std
#include <memory>

namespace eng::rhi {

// Forward declarations
class BufferProtocol;
class CommandListProtocol;
class PipelineProtocol;
class ResourceLayoutProtocol;
class ResourceSetProtocol;
class SwapchainProtocol;
class TextureProtocol;
class RenderTargetProtocol;

struct PipelineConfig;
struct ResourceLayoutConfig;

enum class BufferUsage;
enum class Format;

// =====================
// class ContextProtocol
// =====================

/// This class provides a protocol (pure abstract interface) representing the
/// core graphics context. It is responsible for global GPU state management
/// and acts as a factory for all RHI resources.
class ContextProtocol {
  public:
    // CREATORS

    virtual ~ContextProtocol() = default;

    // MANIPULATORS

    /// Initialize the underlying RHI context. Optionally enable validation
    /// layers if the specified 'enableValidation' is 'true'. Return 'true'
    /// on success, and 'false' otherwise.
    virtual bool initialize(bool enableValidation) = 0;

    /// Block the calling thread until the GPU has finished executing all
    /// submitted commands.
    virtual void waitIdle() = 0;

    /// Create and return a newly allocated swapchain configured with the
    /// specified 'width' and 'height'.
    virtual std::unique_ptr<SwapchainProtocol>
    createSwapchain(uint32_t width, uint32_t height) = 0;

    /// Create and return a newly allocated command list.
    virtual std::unique_ptr<CommandListProtocol> createCommandList() = 0;

    /// Create and return a newly allocated buffer of the specified 'size'
    /// in bytes, configured for the specified 'usage'.
    virtual std::unique_ptr<BufferProtocol>
    createBuffer(size_t size, BufferUsage usage) = 0;

    /// Create and return a newly allocated graphics pipeline configured
    /// according to the specified 'config'.
    virtual std::unique_ptr<PipelineProtocol>
    createPipeline(const PipelineConfig& config) = 0;

    /// Create and return a newly allocated texture with the specified
    /// 'width', 'height', 'mipLevels', 'format', and 'pixels' data.
    virtual std::unique_ptr<TextureProtocol>
    createTexture(uint32_t    width,
                  uint32_t    height,
                  uint32_t    mipLevels,
                  Format      format,
                  const void* pixels) = 0;

    /// Create and return a newly allocated render target with the specified
    /// 'width', 'height', and 'format'.
    virtual std::unique_ptr<RenderTargetProtocol>
    createRenderTarget(uint32_t width, uint32_t height, Format format) = 0;

    /// Create and return a newly allocated resource layout configured
    /// according to the specified 'config'.
    virtual std::unique_ptr<ResourceLayoutProtocol>
    createResourceLayout(const ResourceLayoutConfig& config) = 0;

    /// Create and return a newly allocated resource set bound to the
    /// specified 'layout'. The behavior is undefined if 'layout' is null.
    virtual std::unique_ptr<ResourceSetProtocol>
    createResourceSet(ResourceLayoutProtocol* layout) = 0;
};

}  // close package namespace
#endif  // INCLUDED_ENG_RHI_CONTEXTPROTOCOL_H
