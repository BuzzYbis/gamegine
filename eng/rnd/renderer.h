// renderer.h                                                         -*-C++-*-
#ifndef INCLUDED_ENG_RND_RENDERER_H
#define INCLUDED_ENG_RND_RENDERER_H

//@PURPOSE: Provide a high-level orchestration of the rendering process.
//
//@CLASSES:
//  eng::rnd::Renderer: Main class responsible for the rendering loop.
//
//@DESCRIPTION: This component provides the 'eng::rnd::Renderer' class, which
// acts as the central hub for graphics operations in the engine. It manages
// the Render Hardware Interface (RHI) protocols to initialize the GPU state,
// create pipelines, and orchestrate the per-frame rendering of entities
// from the scene graph.

// std
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// core
#include <core/core_window.h>

// rhi
#include <rhi/rhi_bufferprotocol.h>
#include <rhi/rhi_commandlistprotocol.h>
#include <rhi/rhi_contextprotocol.h>
#include <rhi/rhi_pipelineprotocol.h>
#include <rhi/rhi_resourcelayoutprotocol.h>
#include <rhi/rhi_resourcesetprotocol.h>
#include <rhi/rhi_swapchainprotocol.h>

// renderer
#include <rnd/material.h>

// scene
#include <scn/scn_scene.h>

namespace eng::rnd {

// ==============
// class Renderer
// ==============

/// This class is responsible for managing the high-level rendering logic,
/// interfacing with the RHI to submit commands to the GPU.
class Renderer {
  private:
    // DATA
    core::Window*         d_window_p  = nullptr;
    rhi::ContextProtocol* d_context_p = nullptr;

    std::unique_ptr<rhi::SwapchainProtocol>                 d_swapchain;
    std::vector<std::unique_ptr<rhi::CommandListProtocol> > d_commandLists;

    // Global Resources (Camera)
    std::unique_ptr<rhi::ResourceLayoutProtocol> d_globalLayout;
    std::unique_ptr<rhi::ResourceSetProtocol>    d_globalResourceSet;
    std::unique_ptr<rhi::ResourceLayoutProtocol> d_materialLayout;
    std::unique_ptr<rhi::BufferProtocol>         d_cameraUbo;

    uint32_t d_currentFrame = 0;  // CPU frame index
    uint32_t d_imageIndex   = 0;  // GPU image index

    std::unordered_map<std::string, std::unique_ptr<rhi::PipelineProtocol> >
        d_pipelines;

    bool d_wasResized = false;

  private:
    // PRIVATE MANIPULATORS

    /// Create the internal graphics pipelines (PBR, Wireframe, etc.).
    void createPipelines();

    // PRIVATE ACCESSORS

    /// Record into the specified 'cmd' the draw commands of every mesh of
    /// the specified 'scene' whose material is blended if the specified
    /// 'blended' is 'true', and of every other mesh otherwise.
    void drawMeshes(rhi::CommandListProtocol* cmd,
                    scn::Scene&               scene,
                    bool                      blended) const;

  public:
    // CREATORS

    /// Create a renderer using the specified 'context' and 'window'. The
    /// behavior is undefined unless 'context' and 'window' are non-null and
    /// remain valid for the lifetime of this object.
    explicit Renderer(rhi::ContextProtocol* context, core::Window* window);

    /// Destroy this renderer and release its owned GPU resources.
    ~Renderer() = default;

    // MANIPULATORS

    /// Initialize the swapchain, pipelines, and global resource sets.
    /// Return 'true' on success, and 'false' otherwise.
    bool initialize();

    /// Prepare the engine for a new frame. Return a pointer to the command
    /// list to be used for recording graphics commands. Return null if
    /// the frame preparation fails (e.g., due to a window resize).
    rhi::CommandListProtocol* beginFrame(scn::Scene& scene);

    /// Set the current rendering destination to the swapchain's back buffer.
    void beginSwapchainPass(rhi::CommandListProtocol* cmd) const;

    /// Finalize the current frame, submit command lists to the GPU, and
    /// request presentation to the screen.
    void endFrame(rhi::CommandListProtocol* cmd);

    /// Check if a resize event occurred since the last call. Return 'true'
    /// if the renderer was resized, and 'false' otherwise.
    bool consumeResizeEvent();

    // ACCESSORS

    /// Iterate over the specified 'scene' and record draw commands into the
    /// specified 'cmd' for all renderable entities.
    void renderScene(rhi::CommandListProtocol* cmd, scn::Scene& scene) const;

    /// Calculate and upload the camera matrices from the specified 'scene'
    /// to the GPU uniform buffer.
    void updateUniformBuffer(scn::Scene& scene) const;

    /// Return a reference to the underlying RHI context.
    [[nodiscard]]
    rhi::ContextProtocol& context() const;

    /// Return a pointer to the current swapchain manager.
    [[nodiscard]]
    rhi::SwapchainProtocol* swapchain() const;

    /// Return a pointer to the pipeline state object with the specified
    /// 'name'. Return null if no such pipeline exists.
    [[nodiscard]]
    rhi::PipelineProtocol* getPipeline(const std::string& name) const;

    /// Return a pointer to the pipeline a material of the specified 'mode'
    /// and 'doubleSided' is to be drawn with. Masked and opaque materials
    /// share a pipeline: masking is a discard in the fragment shader, which
    /// no pipeline state expresses.
    [[nodiscard]]
    rhi::PipelineProtocol* materialPipeline(AlphaMode mode,
                                            bool      doubleSided) const;

    /// Return a pointer to the resource layout used for materials.
    [[nodiscard]]
    rhi::ResourceLayoutProtocol* materialLayout() const;
};

// INLINE DEFINITIONS
inline rhi::ContextProtocol& Renderer::context() const
{
    return *d_context_p;
}

inline rhi::SwapchainProtocol* Renderer::swapchain() const
{
    return d_swapchain.get();
}

inline rhi::ResourceLayoutProtocol* Renderer::materialLayout() const
{
    return d_materialLayout.get();
}

}  // close package namespace

#endif  // INCLUDED_ENG_RND_RENDERER_H
