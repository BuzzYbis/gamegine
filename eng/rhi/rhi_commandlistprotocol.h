// rhi_commandlistprotocol.h                                          -*-C++-*-
#ifndef INCLUDED_ENG_RHI_COMMANDLISTPROTOCOL_H
#define INCLUDED_ENG_RHI_COMMANDLISTPROTOCOL_H

//@PURPOSE: Provide a pure abstract interface for recording GPU commands.
//
//@CLASSES:
//  eng::rhi::CommandListProtocol: Protocol for recording graphics commands.
//
//@DESCRIPTION: This component provides a pure abstract interface,
// 'eng::rhi::CommandListProtocol', that represents a sequence of graphics
// commands to be executed by the GPU. It provides methods for setting the
// viewport, binding pipelines and resource sets, and issuing draw calls.
// Command lists are recorded by the CPU and then submitted to a queue for
// asynchronous execution.

// rhi
#include <rhi/rhi_types.h>

namespace eng::rhi {

// Forward declarations
class ResourceSetProtocol;
class PipelineProtocol;
class BufferProtocol;
class SwapchainProtocol;

// =========================
// class CommandListProtocol
// =========================

/// This class provides a protocol (pure abstract interface) representing a
/// list of commands to be executed by the GPU. It encapsulates the recording
/// of state changes and draw operations.
class CommandListProtocol {
  public:
    // CREATORS

    /// Destroy this command list and release any temporary recording
    /// resources.
    virtual ~CommandListProtocol() = default;

    // MANIPULATORS

    /// Start recording commands into this list. The behavior is undefined
    /// if 'begin' is called while the list is already in the recording
    /// state.
    virtual void begin() = 0;

    /// Finish recording commands. The command list must be in the
    /// recording state. After calling 'end', the list can be submitted to
    /// a queue for execution.
    virtual void end() = 0;

    /// Set the viewport transformation parameters for subsequent draw calls.
    virtual void setViewport(float x,
                             float y,
                             float width,
                             float height,
                             float minDepth = 0.0f,
                             float maxDepth = 1.0f) = 0;

    /// Set the scissor rectangle for subsequent draw calls.
    virtual void
    setScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) = 0;

    /// Begin a rendering pass targeting the specified 'imageIndex' of the
    /// specified 'swapchain'. Use the specified 'clearColor' to initialize
    /// the color attachment.
    virtual void beginSwapchainRendering(SwapchainProtocol* swapchain,
                                         uint32_t           imageIndex,
                                         const ClearColor&  clearColor) = 0;

    /// End the current rendering pass targeting the swapchain.
    virtual void endSwapchainRendering(SwapchainProtocol* swapchain,
                                       uint32_t           imageIndex) = 0;

    /// Bind the specified 'pipeline' state object for subsequent draw
    /// calls.
    virtual void bindPipeline(PipelineProtocol* pipeline) = 0;

    /// Issue a non-indexed draw call with the specified 'vertexCount'.
    virtual void draw(uint32_t vertexCount,
                      uint32_t instanceCount,
                      uint32_t firstVertex,
                      uint32_t firstInstance) = 0;

    /// Issue an indexed draw call with the specified 'indexCount'.
    virtual void drawIndexed(uint32_t indexCount,
                             uint32_t instanceCount,
                             uint32_t firstIndex,
                             int32_t  vertexOffset,
                             uint32_t firstInstance) = 0;

    /// Update the push constant data for the specified shader 'stage' in
    /// the specified 'pipeline' using the data pointed to by 'data' of
    /// the specified 'size' in bytes.
    virtual void pushConstants(PipelineProtocol* pipeline,
                               ShaderStage       stage,
                               uint32_t          offset,
                               uint32_t          size,
                               const void*       data) = 0;

    /// Bind the specified 'resourceSet' (containing buffers or textures)
    /// to the specified 'setIndex' for the specified 'pipeline'.
    virtual void bindResourceSet(PipelineProtocol*    pipeline,
                                 uint32_t             setIndex,
                                 ResourceSetProtocol* resourceSet) = 0;

    /// Bind the specified vertex 'buffer' to the specified 'binding' slot.
    virtual void bindVertexBuffer(BufferProtocol* buffer,
                                  uint32_t        binding = 0,
                                  size_t          offset  = 0) = 0;

    /// Bind the specified index 'buffer' for subsequent indexed draw
    /// calls.
    virtual void bindIndexBuffer(BufferProtocol* buffer,
                                 size_t          offset = 0) = 0;
};

}  // close package namespace

#endif  // INCLUDED_ENG_RHI_COMMANDLISTPROTOCOL_H