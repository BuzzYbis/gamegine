// vk_swapchain.cpp                                                   -*-C++-*-
#include <rhi/vulkan/vk_swapchain.h>

// renderer
#include <renderer/renderer.h>

// rhi
#include <rhi/vulkan/vk_resource_utils.h>

// std
#include <iostream>

namespace engine::rhi::vulkan {

VulkanSwapchain::VulkanSwapchain(VulkanContext& context,
                                 const uint32_t width,
                                 const uint32_t height)
: d_context(context)
, d_format()
, d_width(width)
, d_height(height)
{
}

bool VulkanSwapchain::initialize()
{
    try {
        createSwapchain();
        createImageViews();
        createColorResources();
        createDepthResources();
    }
    catch (const std::exception& e) {
        std::cerr << "[VulkanSwapchain] Init failed: " << e.what()
                  << std::endl;
        return false;
    }
    return true;
}

void VulkanSwapchain::createSwapchain()
{
    // Query Surface Capabilities.
    const auto& physDev = d_context.physicalDevice();
    const auto& surface = d_context.surface();

    // Get physical constraints: min/max image count, min/max resolution
    // (extent).
    const auto caps = physDev.getSurfaceCapabilitiesKHR(*surface);
    // Get supported pixel formats (e.g., B8G8R8A8_SRGB, HDR formats).
    const auto formats = physDev.getSurfaceFormatsKHR(*surface);
    // Get available presentation modes (how the presentation engine queues
    // images).
    const auto presentModes = physDev.getSurfacePresentModesKHR(*surface);

    // Select the optimal format (preferred: SRGB with 32-bit color).
    const auto surfaceFormat = chooseSwapSurfaceFormat(formats);
    // Select the presentation strategy (preferred: Mailbox for low latency,
    // fallback to FIFO/V-Sync).
    const auto presentMode = chooseSwapPresentMode(presentModes);

    // Determine the exact resolution of the swapchain images matching the
    // window.
    const auto extent = chooseSwapExtent(caps);

    // Request at least one more image than the minimum to avoid stalling the
    // driver. This allows for Triple Buffering if the hardware supports it.
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo(
        {},
        *surface,
        imageCount,
        surfaceFormat.format,
        surfaceFormat.colorSpace,
        extent,
        1,  // imageArrayLayers (1 for standard 2D, 2 for VR stereo).
        vk::ImageUsageFlagBits::eColorAttachment);

    // Images are owned by one queue family at a time.
    createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    // Apply the current transformation required by the surface.
    // (Usually 'Identity' on PC, but handles screen rotation on
    // mobile/tablets).
    createInfo.preTransform = caps.currentTransform;
    // The window surface is opaque (ignores the alpha channel for window
    // composition).
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    // Set the presentation mode (V-Sync vs Mailbox).
    createInfo.presentMode = presentMode;
    // Enable clipping: discard rendering for pixels obscured by other windows.
    createInfo.clipped = VK_TRUE;
    // Important for resizing: pass the old swapchain handle to the driver.
    // This allows internal resource recycling and ensures a smooth transition.
    createInfo.oldSwapchain = *d_swapchain;

    // Create the new swapchain.
    d_swapchain = vk::raii::SwapchainKHR(d_context.device(), createInfo);

    // Cache the selected format and extent for later use (e.g., by the Camera
    // or Pipeline).
    d_format = surfaceFormat.format;
    d_extent = extent;
}

void VulkanSwapchain::createImageViews()
{
    // Retrieve the handles of the images created by the Swapchain
    const auto images = d_swapchain.getImages();

    // Clear old views in case the window was resized.
    d_imageViews.clear();
    d_imageViews.reserve(images.size());

    // For each raw image, we must create an "ImageView". Vulkan cannot use a
    // raw image directly (it's just a block of memory). The "View" acts like a
    // pair of glasses: it tells the GPU how to read this data (e.g.,
    // "Interpret this as a 2D Texture with RGB colors").
    for (const auto& image : images) {
        vk::ImageViewCreateInfo viewInfo(
            {},
            image,
            vk::ImageViewType::e2D,
            d_format,
            {},
            {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

        d_imageViews.emplace_back(d_context.device(), viewInfo);
    }
}

bool VulkanSwapchain::acquireNextImage(const vk::raii::Semaphore& semaphore,
                                       uint32_t& outImageIndex)
{
    try {
        // Ask the Swapchain for the index of the next available image.
        // IMPORTANT: The 'semaphore' passed here acts as a traffic light for
        // the GPU. The Swapchain will turn it green (Signal) ONLY when the
        // image is completely free and ready to be painted on by our rendering
        // engine.
        auto [result, index] = d_swapchain.acquireNextImage(
            std::numeric_limits<uint64_t>::max(),  // Infinite timeout (we wait
                                                   // for the image)
            *semaphore);

        // If the window size has changed (e.g., user resize),
        // the current Swapchain is no longer valid, so we must recreate it.
        if (result == vk::Result::eErrorOutOfDateKHR) {
            recreateSwapChain();
            return false;  // Skip this frame, we will draw on the next one
        }

        if (result != vk::Result::eSuccess &&
            result != vk::Result::eSuboptimalKHR) {
            std::cerr << "Failed to acquire swap chain image!" << std::endl;
            return false;
        }

        // Just return the index of the image to use in our
        // d_imageViews array.
        outImageIndex = index;
        return true;
    }
    catch ([[maybe_unused]] const std::exception& e) {
        recreateSwapChain();
        return false;
    }
}

void VulkanSwapchain::recreateSwapChain()
{
    int        width = 0, height = 0;
    const auto window = d_context.window()->window();
    glfwGetFramebufferSize(window, &width, &height);

    // Pause execution if the windows is minimized.
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    // Wait for the GPU to finish its current tasks before destroying
    // resources.
    d_context.device().waitIdle();

    // Rebuild the Swapchain and its associated image views.
    d_width  = static_cast<uint32_t>(width);
    d_height = static_cast<uint32_t>(height);

    try {
        createSwapchain();
        createImageViews();
        createColorResources();  // For MSAA
        createDepthResources();  // For Z-Buffering
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to recreate swapchain: " << e.what() << std::endl;
    }
}

vk::SurfaceFormatKHR VulkanSwapchain::chooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR>& formats)
{
    for (const auto& fmt : formats) {
        // We prefer SRGB for automatic gamma-correct rendering
        if (fmt.format == vk::Format::eB8G8R8A8Srgb &&
            fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return fmt;
        }
    }
    return formats[0];  // Fallback to the first available format
}

vk::PresentModeKHR VulkanSwapchain::chooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR>& presentModes)
{
    for (const auto& mode : presentModes) {
        // Mailbox allows low latency and no tearing (Triple Buffering)
        if (mode == vk::PresentModeKHR::eMailbox) {
            return mode;
        }
    }
    // Guaranteed to be available (Standard V-Sync)
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D
VulkanSwapchain::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& caps) const
{
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return caps.currentExtent;
    }

    vk::Extent2D actualExtent = {d_width, d_height};

    actualExtent.width  = std::clamp(actualExtent.width,
                                    caps.minImageExtent.width,
                                    caps.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height,
                                     caps.minImageExtent.height,
                                     caps.maxImageExtent.height);

    return actualExtent;
}

vk::Format VulkanSwapchain::findSupportedFormat(
    const std::vector<vk::Format>& candidates,
    const vk::ImageTiling          tiling,
    const vk::FormatFeatureFlags   features) const
{
    for (const auto format : candidates) {
        vk::FormatProperties props =
            d_context.physicalDevice().getFormatProperties(format);

        if (tiling == vk::ImageTiling::eLinear &&
            (props.linearTilingFeatures & features) == features) {
            return format;
        }
        if (tiling == vk::ImageTiling::eOptimal &&
            (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

vk::Format VulkanSwapchain::findDepthFormat() const
{
    return findSupportedFormat(
        {vk::Format::eD32Sfloat,
         vk::Format::eD32SfloatS8Uint,
         vk::Format::eD24UnormS8Uint},
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

void VulkanSwapchain::createColorResources()
{
    VulkanResourceUtils::createImage(
        d_context,
        d_extent.width,
        d_extent.height,
        1,
        d_context.msaaSamples(),
        d_format,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransientAttachment |
            vk::ImageUsageFlagBits::eColorAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        d_colorImage,
        d_colorImageMemory);
    d_colorImageView = VulkanResourceUtils::createImageView(
        d_context.device(),
        d_colorImage,
        d_format,
        vk::ImageAspectFlagBits::eColor,
        1);
}

void VulkanSwapchain::createDepthResources()
{
    d_depthFormat = findDepthFormat();

    // Allocate the Depth Image in VRAM
    VulkanResourceUtils::createImage(
        d_context,
        d_extent.width,
        d_extent.height,
        1,
        d_context.msaaSamples(),
        d_depthFormat,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        d_depthImage,
        d_depthImageMemory);

    // Create the View to access this image as a Depth attachment
    d_depthImageView = VulkanResourceUtils::createImageView(
        d_context.device(),
        d_depthImage,
        d_depthFormat,
        vk::ImageAspectFlagBits::eDepth,
        1);
}

}  // close engine::rhi::vulkan namespace
