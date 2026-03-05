// command_pool.h                                                     -*-C++-*-
#ifndef INCLUDED_ENGINE_RENDERER_COMMAND_POOL_H
#define INCLUDED_ENGINE_RENDERER_COMMAND_POOL_H

//@PURPOSE: Provide a manager for the Vulkan Command Pool.
//
//@CLASSES:
//  engine::renderer::CommandPool: Manager for the Vulkan Command Pool.
//
//@DESCRIPTION: This component manages the lifecycle of the Vulkan Command
// Pool. It creates the command pool and the command buffers.

#include <rhi/vulkan/vk_context.h>
#include <vulkan/vulkan_raii.hpp>

namespace engine::renderer {
// -----------------
// class CommandPool
// -----------------

/// This class manages the lifecycle of the Vulkan Command Pool.
class CommandPool {
  private:
    // DATA

    rhi::vulkan::VulkanContext& d_context;
    vk::raii::CommandPool       d_commandPool = nullptr;

  public:
    // CREATORS

    explicit CommandPool(rhi::vulkan::VulkanContext& context);

    [[nodiscard]] const vk::raii::CommandPool& commandPool() const;
    [[nodiscard]] vk::raii::CommandPool&       commandPool();
};
// =======================================================================
//                          INLINE DEFINITIONS
// =======================================================================

inline const vk::raii::CommandPool& CommandPool::commandPool() const
{
    return d_commandPool;
}

inline vk::raii::CommandPool& CommandPool::commandPool()
{
    return d_commandPool;
}

}  // close engine::renderer namespace

#endif  // INCLUDED_ENGINE_RENDERER_COMMAND_POOL_H
