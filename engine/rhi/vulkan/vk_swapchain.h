// vk_swapchain.h                                                     -*-C++-*-
#ifndef INCLUDED_ENGINE_RHI_VULKAN_VK_SWAPCHAIN_H
#define INCLUDED_ENGINE_RHI_VULKAN_VK_SWAPCHAIN_H

//@PURPOSE: Provide a manager for the Vulkan Swapchain.
//
//@CLASSES:
//  engine::rhi::vulkan::VulkanSwapchain: RAII mechanism for the Vulkan
//  Swapchain.
//
//@DESCRIPTION: This component manages the lifecycle of the Vulkan Swapchain.
// It creates the swapchain, the image views, the framebuffers, the depth
// image, the color image, and the multisampling image. It also manages the
// recreation of the swapchain when the window is resized.

// rhi
#include <rhi/vulkan/vk_context.h>

// Forward declaration
namespace engine::renderer {

class Renderer;

}  // close engine::renderer namespace

namespace engine::rhi::vulkan {

// ---------------------
// class VulkanSwapchain
// ---------------------

/// RAII manager for the Vulkan swapchain and associated render targets.
///
/// Owns the swapchain object, per-image views, framebuffers, and optional
/// depth and MSAA color attachments. Use extent() or width()/height() for
/// projection and viewport dimensions (e.g. aspect ratio).
class VulkanSwapchain {
  private:
    // DATA

    /// Vulkan device and instance reference.
    VulkanContext& d_context;

    vk::raii::SwapchainKHR d_swapchain = nullptr;

    /// Pixel format of swapchain images (how colors are stored).
    vk::Format d_format;

    /// Framebuffer dimensions in pixels; use for viewport and projection.
    vk::Extent2D d_extent;

    /// One image view per swapchain image, used as render target attachments.
    std::vector<vk::raii::ImageView> d_imageViews;

    // Depth resources (single depth buffer, shared across framebuffers)
    vk::raii::Image        d_depthImage       = nullptr;
    vk::raii::DeviceMemory d_depthImageMemory = nullptr;
    vk::raii::ImageView    d_depthImageView   = nullptr;
    vk::Format             d_depthFormat      = vk::Format::eUndefined;

    // Multisampling (MSAA) resolve target
    vk::raii::Image        d_colorImage       = nullptr;
    vk::raii::DeviceMemory d_colorImageMemory = nullptr;
    vk::raii::ImageView    d_colorImageView   = nullptr;

    uint32_t d_width;   ///< Cached width (same as extent().width).
    uint32_t d_height;  ///< Cached height (same as extent().height).

    // PRIVATE CLASS METHODS

    /// Selects the color format and color space for the images.
    /// Prefers a 32-bit SRGB format to ensure colors are displayed
    /// accurately and gamma-corrected on standard monitors.
    vk::SurfaceFormatKHR
    chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats);

    /// Determines the V-Sync strategy (presentation mode).
    /// Prefers Mailbox (Triple Buffering) for the lowest latency without
    /// tearing. Falls back to FIFO (Standard V-Sync) which is guaranteed by
    /// the hardware.
    vk::PresentModeKHR
    chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& presentModes);

    /// Calculates the exact resolution of the swapchain images.
    /// Ensures the size matches the OS window, clamping the dimensions
    /// to handle special cases like High-DPI (Retina) displays.
    vk::Extent2D
    chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& caps) const;

    /// Allocates and creates MSAA color image and view.
    void createColorResources();

    /// Allocates and creates depth image, memory, and view.
    void createDepthResources();

    /// Returns a depth format supported for depth/stencil attachment.
    [[nodiscard]] vk::Format findDepthFormat() const;

    /// Returns first format in candidates that supports given tiling and
    /// features.
    [[nodiscard]] vk::Format
    findSupportedFormat(const std::vector<vk::Format>& candidates,
                        vk::ImageTiling                tiling,
                        vk::FormatFeatureFlags         features) const;

  public:
    // CREATORS

    /// Constructs the swapchain manager for the given context and dimensions.
    /// Call initialize() before using the swapchain.
    VulkanSwapchain(VulkanContext& context, uint32_t width, uint32_t height);

    // MANIPULATORS

    /// Creates swapchain, image views, framebuffers, depth and color
    /// resources. Returns false if creation fails (e.g. window minimized).
    bool initialize();

    /// (Re)creates the swapchain; used internally and by recreateSwapChain().
    void createSwapchain();

    /// Creates one image view per swapchain image.
    void createImageViews();

    /// Acquires the next image index for rendering; signals semaphore when
    /// ready.
    /// \return true if an image was acquired, false on suboptimal/out-of-date.
    bool acquireNextImage(const vk::raii::Semaphore& semaphore,
                          uint32_t&                  outImageIndex);

    /// Rebuilds the swapchain and all dependent resources.
    /// Pauses execution if the window is minimized (size 0,0) and handles
    /// the recreation of views, depth, and color resources after a resize.
    void recreateSwapChain();

    // ACCESSORS

    /// Pixel format of swapchain images.
    [[nodiscard]] vk::Format format() const;

    /// Framebuffer extent (width x height); use for viewport and aspect ratio.
    [[nodiscard]] vk::Extent2D extent() const;

    /// Image views for all swapchain images.
    [[nodiscard]] const std::vector<vk::raii::ImageView>& imageViews() const;

    /// Underlying Vulkan swapchain handle.
    [[nodiscard]] const vk::raii::SwapchainKHR& swapchain() const;

    /// Raw VkImage handles for all swapchain images.
    [[nodiscard]] std::vector<vk::Image> images() const;

    /// Framebuffer width in pixels.
    [[nodiscard]] const uint32_t& width() const;

    /// Framebuffer height in pixels.
    [[nodiscard]] const uint32_t& height() const;

    /// Depth attachment image used by framebuffers.
    [[nodiscard]] const vk::raii::Image& depthImage() const;

    /// Image view for the depth attachment.
    [[nodiscard]] const vk::raii::ImageView& depthImageView() const;

    /// Pixel format of the depth image.
    [[nodiscard]] vk::Format depthFormat() const;

    /// MSAA color resolve image view (when multisampling is enabled).
    [[nodiscard]] const vk::raii::ImageView& colorImageView() const;
};

// =======================================================================
//                          INLINE DEFINITIONS
// =======================================================================

// ACCESSORS

inline vk::Format VulkanSwapchain::format() const
{
    return d_format;
}

inline vk::Extent2D VulkanSwapchain::extent() const
{
    return d_extent;
}

inline const uint32_t& VulkanSwapchain::width() const
{
    return d_width;
}

inline const uint32_t& VulkanSwapchain::height() const
{
    return d_height;
}

inline const vk::raii::Image& VulkanSwapchain::depthImage() const
{
    return d_depthImage;
}

inline const vk::raii::ImageView& VulkanSwapchain::depthImageView() const
{
    return d_depthImageView;
}

inline vk::Format VulkanSwapchain::depthFormat() const
{
    return d_depthFormat;
}

inline const vk::raii::ImageView& VulkanSwapchain::colorImageView() const
{
    return d_colorImageView;
}

inline std::vector<vk::Image> VulkanSwapchain::images() const
{
    return d_swapchain.getImages();
}

inline const std::vector<vk::raii::ImageView>&
VulkanSwapchain::imageViews() const
{
    return d_imageViews;
}

inline const vk::raii::SwapchainKHR& VulkanSwapchain::swapchain() const
{
    return d_swapchain;
}

}  // close engine::rhi::vulkan namespace

#endif  // INCLUDED_ENGINE_RHI_VULKAN_VK_SWAPCHAIN_H