// vlk_commandlist.cpp                                                -*-C++-*-
#include <rhi/vlk/vlk_commandlist.h>

// rhi
#include <rhi/rhi_types.h>
#include <rhi/vlk/vlk_buffer.h>
#include <rhi/vlk/vlk_pipeline.h>
#include <rhi/vlk/vlk_resourceset.h>
#include <rhi/vlk/vlk_swapchain.h>
#include <rhi/vlk/vlk_texture.h>

namespace eng::rhi::vlk {

namespace {

vk::ShaderStageFlags getVkShaderStageFlags(const ShaderStage stage)
{
    switch (stage) {
    case ShaderStage::Vertex: return vk::ShaderStageFlagBits::eVertex;
    case ShaderStage::Fragment: return vk::ShaderStageFlagBits::eFragment;
    default: return vk::ShaderStageFlagBits::eAllGraphics;
    }
}

}

CommandList::CommandList(Context* context)
: d_context_p(context)
, d_commandBuffer(nullptr)
{
    // Configure allocation: 1 primary buffer from the context pool.
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool        = *d_context_p->commandPool();
    allocInfo.level              = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = 1;

    // vulkan.hpp always returns a std::vector of buffers.
    // We extract the first one and move it into our member variable.
    d_commandBuffer = std::move(
        vk::raii::CommandBuffers(d_context_p->device(), allocInfo).front());
}

void CommandList::begin()
{
    // Start recording.
    const vk::CommandBufferBeginInfo beginInfo{};
    d_commandBuffer.begin(beginInfo);
}

void CommandList::end()
{
    d_commandBuffer.end();
}

void CommandList::bindPipeline(PipelineProtocol* pipeline)
{
    const auto* vlkPipeline = static_cast<Pipeline*>(pipeline);
    d_commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                 *vlkPipeline->pipeline());
}

void CommandList::draw(const uint32_t vertexCount,
                       const uint32_t instanceCount,
                       const uint32_t firstVertex,
                       const uint32_t firstInstance)
{
    d_commandBuffer.draw(vertexCount,
                         instanceCount,
                         firstVertex,
                         firstInstance);
}

void CommandList::drawIndexed(const uint32_t indexCount,
                              const uint32_t instanceCount,
                              const uint32_t firstIndex,
                              const int32_t  vertexOffset,
                              const uint32_t firstInstance)
{
    d_commandBuffer.drawIndexed(indexCount,
                                instanceCount,
                                firstIndex,
                                vertexOffset,
                                firstInstance);
}

void CommandList::setViewport(const float x,
                              const float y,
                              const float width,
                              const float height,
                              const float minDepth,
                              const float maxDepth)
{
    // The Y axis in Vulkan is reversed compared to OpenGL.
    vk::Viewport viewport = {};
    viewport.width        = width;
    viewport.height       = -height;  // Negate height to flip vertically
    viewport.x            = x;
    viewport.y            = y + height;  // Shift Y down by the original height
    viewport.minDepth     = minDepth;
    viewport.maxDepth     = maxDepth;

    d_commandBuffer.setViewport(0, viewport);
}

void CommandList::setScissor(const int32_t  x,
                             const int32_t  y,
                             const uint32_t width,
                             const uint32_t height)
{
    const vk::Rect2D scissor{vk::Offset2D{x, y}, vk::Extent2D{width, height}};
    d_commandBuffer.setScissor(0, scissor);
}

void CommandList::beginSwapchainRendering(SwapchainProtocol* swapchain,
                                          uint32_t           imageIndex,
                                          const ClearColor&  clearColor)
{
    const auto*         vlkSwapchain   = static_cast<Swapchain*>(swapchain);
    const vk::Image     swapchainImage = vlkSwapchain->image(imageIndex);
    const vk::ImageView swapchainView  = vlkSwapchain->imageView(imageIndex);

    // 1. Barrier for Color (Undefined -> ColorAttachmentOptimal)
    vk::ImageMemoryBarrier colorBarrier{};
    colorBarrier.oldLayout        = vk::ImageLayout::eUndefined;
    colorBarrier.newLayout        = vk::ImageLayout::eColorAttachmentOptimal;
    colorBarrier.image            = swapchainImage;
    colorBarrier.subresourceRange = {vk::ImageAspectFlagBits::eColor,
                                     0,
                                     1,
                                     0,
                                     1};
    colorBarrier.dstAccessMask    = vk::AccessFlagBits::eColorAttachmentWrite;

    d_commandBuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        {},
        nullptr,
        nullptr,
        colorBarrier);

    // 2. Barrier for Depth
    vk::ImageMemoryBarrier depthBarrier{};
    depthBarrier.oldLayout        = vk::ImageLayout::eUndefined;
    depthBarrier.newLayout        = vk::ImageLayout::eDepthAttachmentOptimal;
    depthBarrier.image            = vlkSwapchain->depthImage();
    depthBarrier.subresourceRange = {vk::ImageAspectFlagBits::eDepth,
                                     0,
                                     1,
                                     0,
                                     1};
    depthBarrier.dstAccessMask =
        vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    d_commandBuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eEarlyFragmentTests |
            vk::PipelineStageFlagBits::eLateFragmentTests,
        vk::PipelineStageFlagBits::eEarlyFragmentTests |
            vk::PipelineStageFlagBits::eLateFragmentTests,
        {},
        nullptr,
        nullptr,
        depthBarrier);

    // 3. Attachments
    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.imageView   = swapchainView;
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp      = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp     = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue  = vk::ClearColorValue(clearColor.r,
                                                     clearColor.g,
                                                     clearColor.b,
                                                     clearColor.a);

    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.imageView   = vlkSwapchain->depthImageView();
    depthAttachment.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depthAttachment.loadOp      = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp     = vk::AttachmentStoreOp::eDontCare;
    depthAttachment.clearValue  = vk::ClearDepthStencilValue(1.0f, 0);

    // 4. Begin Rendering
    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea = vk::Rect2D{
        {0, 0},
        vk::Extent2D{vlkSwapchain->width(), vlkSwapchain->height()}};
    renderingInfo.layerCount           = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments    = &colorAttachment;
    renderingInfo.pDepthAttachment     = &depthAttachment;

    d_commandBuffer.beginRendering(renderingInfo);
}

void CommandList::endSwapchainRendering(SwapchainProtocol* swapchain,
                                        uint32_t           imageIndex)
{
    // 1. D'ABORD LE END RENDERING ! (On signale qu'on a fini de peindre)
    d_commandBuffer.endRendering();

    // 2. ENSUITE LA BARRIÈRE ! (On envoie l'image à l'écran)
    const auto*     vlkSwapchain = static_cast<Swapchain*>(swapchain);
    const vk::Image image        = vlkSwapchain->image(imageIndex);

    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout        = vk::ImageLayout::eColorAttachmentOptimal;
    barrier.newLayout        = vk::ImageLayout::ePresentSrcKHR;
    barrier.image            = image;
    barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    barrier.srcAccessMask    = vk::AccessFlagBits::eColorAttachmentWrite;

    d_commandBuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eBottomOfPipe,
        {},
        nullptr,
        nullptr,
        barrier);
}

void CommandList::pushConstants(PipelineProtocol* pipeline,
                                ShaderStage       stage,
                                uint32_t          offset,
                                uint32_t          size,
                                const void*       data)
{
    const auto*                vlkPipeline = static_cast<Pipeline*>(pipeline);
    const vk::ShaderStageFlags vkStage     = getVkShaderStageFlags(stage);

    const vk::ArrayProxy<const uint8_t> pushData(
        size,
        static_cast<const uint8_t*>(data));

    d_commandBuffer.pushConstants<uint8_t>(*vlkPipeline->layout(),
                                           vkStage,
                                           offset,
                                           pushData);
}

void CommandList::bindResourceSet(PipelineProtocol*    pipeline,
                                  uint32_t             setIndex,
                                  ResourceSetProtocol* resourceSet)
{
    const auto* vlkPipeline = static_cast<Pipeline*>(pipeline);
    const auto* vlkSet      = static_cast<ResourceSet*>(resourceSet);

    d_commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                       *vlkPipeline->layout(),
                                       setIndex,
                                       *vlkSet->set(),
                                       {});
}

void CommandList::bindVertexBuffer(BufferProtocol* buffer,
                                   uint32_t        binding,
                                   size_t          offset)
{
    auto*          vlkBuffer = static_cast<Buffer*>(buffer);
    vk::Buffer     vkBuf     = *vlkBuffer->buffer();
    vk::DeviceSize vkOffset  = offset;

    d_commandBuffer.bindVertexBuffers(binding, vkBuf, vkOffset);
}

void CommandList::bindIndexBuffer(BufferProtocol* buffer, size_t offset)
{
    auto* vlkBuffer = static_cast<Buffer*>(buffer);

    d_commandBuffer.bindIndexBuffer(*vlkBuffer->buffer(),
                                    offset,
                                    vk::IndexType::eUint32);
}

}  // close package namespace
