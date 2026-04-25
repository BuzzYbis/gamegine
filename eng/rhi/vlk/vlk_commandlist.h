// vlk_commandlist.h                                                  -*-C++-*-
#ifndef INCLUDED_ENG_RHI_VLK_COMMANDLIST_H
#define INCLUDED_ENG_RHI_VLK_COMMANDLIST_H

//@PURPOSE: Provide a Vulkan-specific implementation for recording GPU commands.
//
//@CLASSES:
//  eng::rhi::vlk::CommandList: Vulkan backend for 'CommandListProtocol'.
//
//@DESCRIPTION: This component provides a class, 'eng::rhi::vlk::CommandList',
// that implements the 'eng::rhi::CommandListProtocol' for the Vulkan
// backend. It encapsulates a 'vk::CommandBuffer', providing methods to record
// graphics operations such as pipeline binding, resource updates, and draw
// calls for execution on the GPU.

// rhi
#include <rhi/rhi_commandlistprotocol.h>

// vlk
#include <rhi/vlk/vlk_context.h>

namespace eng::rhi {
class PipelineProtocol;
}  // close package namespace

namespace eng::rhi::vlk {

// =================
// class CommandList
// =================

/// This class implements the command list protocol for the Vulkan backend.
class CommandList : public CommandListProtocol {
  private:
    // DATA

    /// Pointer to the Vulkan context (held, not owned).
    Context* d_context_p;

    /// The actual Vulkan command buffer used to record GPU instructions.
    vk::raii::CommandBuffer d_commandBuffer;

  public:
    // CREATORS

    /// Allocate a new command buffer from the specified 'context' command
    /// pool. The behavior is undefined unless 'context' is valid.
    explicit CommandList(Context* context);

    /// Destroy this command list and release the Vulkan handle.
    ~CommandList() override = default;

    // MANIPULATORS

    /// Start recording commands into the internal Vulkan buffer.
    void begin() override;

    /// Finish recording commands.
    void end() override;

    /// Bind the specified 'pipeline' state object for subsequent draw
    /// calls.
    void bindPipeline(PipelineProtocol* pipeline) override;

    /// Issue a non-indexed draw call with the specified parameters.
    void draw(uint32_t vertexCount,
              uint32_t instanceCount,
              uint32_t firstVertex,
              uint32_t firstInstance) override;

    /// Issue an indexed draw call with the specified parameters.
    void drawIndexed(uint32_t indexCount,
                     uint32_t instanceCount,
                     uint32_t firstIndex,
                     int32_t  vertexOffset,
                     uint32_t firstInstance) override;

    /// Set the viewport transformation parameters.
    void setViewport(float x,
                     float y,
                     float width,
                     float height,
                     float minDepth = 0.0f,
                     float maxDepth = 1.0f) override;

    /// Set the scissor rectangle.
    void
    setScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) override;

    /// Begin a rendering pass targeting the specified 'imageIndex' of the
    /// specified 'swapchain'.
    void beginSwapchainRendering(SwapchainProtocol* swapchain,
                                 uint32_t           imageIndex,
                                 const ClearColor&  clearColor) override;

    /// End the current rendering pass targeting the swapchain.
    void endSwapchainRendering(SwapchainProtocol* swapchain,
                               uint32_t           imageIndex) override;

    /// Bind the specified 'resourceSet' to the specified 'setIndex' for
    /// the specified 'pipeline'.
    void bindResourceSet(PipelineProtocol*    pipeline,
                         uint32_t             setIndex,
                         ResourceSetProtocol* resourceSet) override;

    /// Update push constant data.
    void pushConstants(PipelineProtocol* pipeline,
                       ShaderStage       stage,
                       uint32_t          offset,
                       uint32_t          size,
                       const void*       data) override;

    /// Bind the specified index 'buffer'.
    void bindIndexBuffer(BufferProtocol* buffer, size_t offset) override;

    /// Bind the specified vertex 'buffer' to the specified 'binding' slot.
    void bindVertexBuffer(BufferProtocol* buffer,
                          uint32_t        binding = 0,
                          size_t          offset  = 0) override;

    // ACCESSORS

    /// Return a const reference to the underlying Vulkan command buffer handle.
    [[nodiscard]]
    const vk::raii::CommandBuffer& commandBuffer() const;
};

// INLINE DEFINITIONS

inline const vk::raii::CommandBuffer& CommandList::commandBuffer() const
{
    return d_commandBuffer;
}

}  // close package namespace
#endif  // INCLUDED_ENG_RHI_VLK_COMMANDLIST_H