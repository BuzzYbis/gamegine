// vlk_swapchain.cpp                                                  -*-C++-*-
#include <rhi/vlk/vlk_swapchain.h>

// rhi
#include <rhi/vlk/vlk_commandlist.h>
#include <rhi/vlk/vlk_utils.h>

// std
#include <algorithm>
#include <iostream>

namespace eng::rhi::vlk {
namespace {

vk::SurfaceFormatKHR
chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats)
{
    for (const auto& fmt : formats) {
        if (fmt.format == vk::Format::eB8G8R8A8Srgb &&
            fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return fmt;
        }
    }
    return formats[0];
}

vk::PresentModeKHR
chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& presentModes)
{
    for (const auto& mode : presentModes) {
        if (mode == vk::PresentModeKHR::eMailbox) {
            return mode;
        }
    }
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& caps,
                              const uint32_t                    width,
                              const uint32_t                    height)
{
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return caps.currentExtent;
    }

    vk::Extent2D actualExtent = {width, height};
    actualExtent.width        = std::clamp(actualExtent.width,
                                    caps.minImageExtent.width,
                                    caps.maxImageExtent.width);
    actualExtent.height       = std::clamp(actualExtent.height,
                                     caps.minImageExtent.height,
                                     caps.maxImageExtent.height);

    return actualExtent;
}

}  // close anonymous namespace

Swapchain::Swapchain(Context*       context,
                     const uint32_t width,
                     const uint32_t height)
: d_context_p(context)
, d_width(width)
, d_height(height)
, d_swapchain(nullptr)
, d_format()
, d_currentFrame(0)
, d_depthImage(nullptr)
, d_depthImageMemory(nullptr)
, d_depthImageView(nullptr)
, d_depthFormat(vk::Format::eUndefined)
, d_colorImage(nullptr)
, d_colorImageMemory(nullptr)
, d_colorImageView(nullptr)
{
}

bool Swapchain::initialize()
{
    try {
        createSwapchain();
        createImageViews();
        createColorResources();
        createDepthResources();
        createSyncObjects();
    }
    catch (const std::exception& e) {
        std::cerr << "[Critical] Swapchain initialization failed: " << e.what()
                  << std::endl;
        return false;
    }
    return true;
}

void Swapchain::createSwapchain()
{
    const auto& physDev = d_context_p->physicalDevice();
    const auto& surface = d_context_p->surface();

    // Query physical constraints and capabilities
    const auto caps         = physDev.getSurfaceCapabilitiesKHR(*surface);
    const auto formats      = physDev.getSurfaceFormatsKHR(*surface);
    const auto presentModes = physDev.getSurfacePresentModesKHR(*surface);

    // Select optimal swapchain settings
    const auto surfaceFormat = chooseSwapSurfaceFormat(formats);
    const auto presentMode   = chooseSwapPresentMode(presentModes);
    const auto extent        = chooseSwapExtent(caps, d_width, d_height);

    // Determine image count (Triple Buffering support)
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    // Create Swapchain
    vk::SwapchainCreateInfoKHR createInfo(
        {},
        *surface,
        imageCount,
        surfaceFormat.format,
        surfaceFormat.colorSpace,
        extent,
        1,
        vk::ImageUsageFlagBits::eColorAttachment);

    createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    createInfo.preTransform     = caps.currentTransform;
    createInfo.compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode      = presentMode;
    createInfo.clipped          = VK_TRUE;
    createInfo.oldSwapchain     = *d_swapchain;

    d_swapchain = vk::raii::SwapchainKHR(d_context_p->device(), createInfo);

    d_format = surfaceFormat.format;
    d_extent = extent;
}

void Swapchain::createImageViews()
{
    d_images = d_swapchain.getImages();
    d_imageViews.clear();
    d_imageViews.reserve(d_images.size());

    for (const auto& image : d_images) {
        vk::ImageViewCreateInfo viewInfo(
            {},
            image,
            vk::ImageViewType::e2D,
            d_format,
            {},
            {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

        d_imageViews.emplace_back(d_context_p->device(), viewInfo);
    }
}

std::optional<uint32_t> Swapchain::acquireNextImage()
{
    try {
        // Wait for CPU/GPU synchronization (Fence)
        [[maybe_unused]] auto waitRes = d_context_p->device().waitForFences(
            *d_inFlightFences[d_currentFrame],
            VK_TRUE,
            std::numeric_limits<uint64_t>::max());

        // Acquire image and signal semaphore
        auto [result, index] = d_swapchain.acquireNextImage(
            std::numeric_limits<uint64_t>::max(),
            *d_imageAvailableSemaphores[d_currentFrame]);

        if (result == vk::Result::eErrorOutOfDateKHR) {
            recreateSwapchain();
            return std::nullopt;
        }

        if (result != vk::Result::eSuccess &&
            result != vk::Result::eSuboptimalKHR) {
            std::cerr << "[Error] Failed to acquire swapchain image!"
                      << std::endl;
            return std::nullopt;
        }

        return std::optional(index);
    }
    catch (const std::exception&) {
        recreateSwapchain();
        return std::nullopt;
    }
}

bool Swapchain::submitAndPresent(CommandListProtocol* cmd,
                                 const uint32_t       imageIndex)
{
    // Submit Graphics Command Buffer
    constexpr vk::PipelineStageFlags waitStages[] = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput};

    vk::SubmitInfo submitInfo{};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &*d_imageAvailableSemaphores[d_currentFrame];
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &*d_renderFinishedSemaphores[imageIndex];

    const auto*             vlkCmd = static_cast<CommandList*>(cmd);
    const vk::CommandBuffer rawCmd = *vlkCmd->commandBuffer();

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &rawCmd;

    d_context_p->device().resetFences(*d_inFlightFences[d_currentFrame]);
    d_context_p->graphicsQueue().submit(submitInfo,
                                        *d_inFlightFences[d_currentFrame]);

    // Present Image to Screen
    vk::PresentInfoKHR presentInfo{};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &*d_renderFinishedSemaphores[imageIndex];
    presentInfo.swapchainCount     = 1;

    const vk::SwapchainKHR swapchain = *d_swapchain;
    presentInfo.pSwapchains          = &swapchain;
    presentInfo.pImageIndices        = &imageIndex;

    try {
        const vk::Result presentResult =
            d_context_p->graphicsQueue().presentKHR(presentInfo);
        if (presentResult == vk::Result::eSuboptimalKHR) {
            recreateSwapchain();
        }
    }
    catch (const vk::OutOfDateKHRError&) {
        recreateSwapchain();
    }
    catch (const std::exception& e) {
        std::cerr << "[Error] Unexpected error during presentation: "
                  << e.what() << std::endl;
        return false;
    }

    // Advance Frame
    d_currentFrame = (d_currentFrame + 1) %
                     static_cast<uint32_t>(d_inFlightFences.size());

    return true;
}

void Swapchain::recreateSwapchain()
{
    int        width = 0, height = 0;
    const auto window = d_context_p->window()->window();
    glfwGetFramebufferSize(window, &width, &height);

    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    d_context_p->waitIdle();

    d_width  = static_cast<uint32_t>(width);
    d_height = static_cast<uint32_t>(height);

    try {
        createSwapchain();
        createImageViews();
        createColorResources();
        createDepthResources();
    }
    catch (const std::exception& e) {
        std::cerr << "[Error] Failed to recreate swapchain: " << e.what()
                  << std::endl;
    }
}

void Swapchain::createColorResources()
{
    Utils::createImage(
        d_context_p,
        d_extent.width,
        d_extent.height,
        1,
        d_context_p->msaaSamples(),
        d_format,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransientAttachment |
            vk::ImageUsageFlagBits::eColorAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        d_colorImage,
        d_colorImageMemory);

    d_colorImageView = Utils::createImageView(
        d_context_p->device(),
        d_colorImage,
        d_format,
        vk::ImageAspectFlagBits::eColor,
        1);
}

void Swapchain::createSyncObjects()
{
    d_imageAvailableSemaphores.clear();
    d_renderFinishedSemaphores.clear();
    d_inFlightFences.clear();

    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo     fenceInfo(vk::FenceCreateFlagBits::eSignaled);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        d_imageAvailableSemaphores.emplace_back(d_context_p->device(),
                                                semaphoreInfo);
        d_inFlightFences.emplace_back(d_context_p->device(), fenceInfo);
    }

    for (size_t i = 0; i < d_images.size(); i++) {
        d_renderFinishedSemaphores.emplace_back(d_context_p->device(),
                                                semaphoreInfo);
    }
}

void Swapchain::createDepthResources()
{
    d_depthFormat = Utils::findDepthFormat(d_context_p->physicalDevice());

    Utils::createImage(
        d_context_p,
        d_extent.width,
        d_extent.height,
        1,
        d_context_p->msaaSamples(),
        d_depthFormat,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        d_depthImage,
        d_depthImageMemory);

    d_depthImageView = Utils::createImageView(
        d_context_p->device(),
        d_depthImage,
        d_depthFormat,
        vk::ImageAspectFlagBits::eDepth,
        1);
}

rhi::Format Swapchain::format() const
{
    if (d_format == vk::Format::eB8G8R8A8Srgb) {
        return rhi::Format::B8G8R8A8_SRGB;
    }
    if (d_format == vk::Format::eR8G8B8A8Srgb) {
        return rhi::Format::R8G8B8A8_SRGB;
    }
    return rhi::Format::Undefined;
}

}  // close package namespace
