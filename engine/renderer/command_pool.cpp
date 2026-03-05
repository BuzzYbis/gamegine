// command_pool.cpp                                                   -*-C++-*-
#include <renderer/command_pool.h>

namespace engine::renderer {

CommandPool::CommandPool(rhi::vulkan::VulkanContext& context)
: d_context(context)
{
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = d_context.graphicsQueueFamilyIndex();

    d_commandPool = vk::raii::CommandPool(d_context.device(), poolInfo);
}
}  // close engine::renderer namespace
