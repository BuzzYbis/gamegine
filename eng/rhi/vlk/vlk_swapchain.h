// vlk_swapchain.h                                                    -*-C++-*-
#ifndef INCLUDED_ENG_RHI_VLK_SWAPCHAIN_H
#define INCLUDED_ENG_RHI_VLK_SWAPCHAIN_H

//@PURPOSE: Provide a Vulkan-specific implementation of the swapchain.
//
//@CLASSES:
//  eng::rhi::vlk::Swapchain: Vulkan implementation of SwapchainProtocol.
//
//@DESCRIPTION: This component provides a class, 'eng::rhi::vlk::Swapchain',
// that implements the 'eng::rhi::SwapchainProtocol'. It manages the
// Vulkan swapchain ('vk::SwapchainKHR'), the presentation images, the depth
// and color resources for multisampling, and the synchronization primitives
// (fences and semaphores) required for safe CPU/GPU rendering loops.

// rhi
#include <rhi/rhi_swapchainprotocol.h>
#include <rhi/vlk/vlk_context.h>

// std
#include <vector>

namespace eng::rhi::vlk {

// ===============
// class Swapchain
// ===============

/// This class implements the RHI swapchain protocol for the Vulkan backend.
/// It is responsible for creating the surface swapchain, managing framebuffers
/// (color and depth), and handling frame synchronization.
class Swapchain : public SwapchainProtocol {
  private:
    // DATA

    /// Pointer to the Vulkan context used for resource creation.
    Context* d_context_p;

    /// Width of the swapchain images in pixels.
    uint32_t d_width;

    /// Height of the swapchain images in pixels.
    uint32_t d_height;

    /// Wrapped Vulkan swapchain handle.
    vk::raii::SwapchainKHR d_swapchain;

    /// Pixel format of the swapchain images.
    vk::Format d_format;

    /// 2D extent (width and height) of the swapchain images.
    vk::Extent2D d_extent;

    /// Collection of raw Vulkan image handles acquired from the swapchain.
    std::vector<vk::Image> d_images;

    /// Collection of image views corresponding to the swapchain images,
    /// used as render target attachments.
    std::vector<vk::raii::ImageView> d_imageViews;

    /// Semaphores signaling when an image is ready to be drawn into.
    std::vector<vk::raii::Semaphore> d_imageAvailableSemaphores;

    /// Semaphores signaling when drawing is complete and ready for
    /// presentation.
    std::vector<vk::raii::Semaphore> d_renderFinishedSemaphores;

    /// Fences to synchronize CPU and GPU, preventing the CPU from submitting
    /// commands too fast.
    std::vector<vk::raii::Fence> d_inFlightFences;

    /// Index of the current frame in flight used for synchronization.
    uint32_t d_currentFrame;

    /// Wrapped Vulkan image for the shared depth buffer.
    vk::raii::Image d_depthImage;

    /// Device memory allocated for the depth buffer.
    vk::raii::DeviceMemory d_depthImageMemory;

    /// Image view to access the depth buffer during rendering.
    vk::raii::ImageView d_depthImageView;

    /// Pixel format of the depth buffer.
    vk::Format d_depthFormat;

    /// Wrapped Vulkan image used as the MSAA color resolve target.
    vk::raii::Image d_colorImage;

    /// Device memory allocated for the MSAA color target.
    vk::raii::DeviceMemory d_colorImageMemory;

    /// Image view to access the MSAA color target.
    vk::raii::ImageView d_colorImageView;

    /// Maximum number of frames processed concurrently.
    /// Set to 3 to handle macOS/MoltenVK forced triple-buffering smoothly.
    static constexpr int MAX_FRAMES_IN_FLIGHT = 3;

    // PRIVATE MANIPULATORS

    /// Create the underlying Vulkan swapchain object.
    void createSwapchain();

    /// Create one image view for each swapchain image.
    void createImageViews();

    /// Create the semaphores and fences required for frame synchronization.
    void createSyncObjects();

    /// Allocate memory and create the MSAA color image and its view.
    void createColorResources();

    /// Allocate memory and create the depth image and its view.
    void createDepthResources();

  public:
    // CREATORS

    /// Create a Vulkan swapchain associated with the specified 'context',
    /// configured with the specified 'width' and 'height'. The behavior is
    /// undefined unless 'context' remains valid for the lifetime of this
    /// object.
    explicit Swapchain(Context* context, uint32_t width, uint32_t height);

    /// Destroy this swapchain and release all associated Vulkan resources.
    ~Swapchain() override = default;

    // MANIPULATORS

    /// Initialize the swapchain, image views, depth/color resources, and
    /// synchronization objects. Return 'true' on success, and 'false' if
    /// creation fails (e.g., if the window is minimized).
    bool initialize() override;

    /// Acquire the next available image from the swapchain for rendering,
    /// signaling the corresponding semaphore when ready. Return the index of
    /// the acquired image on success, and 'std::nullopt' otherwise.
    std::optional<uint32_t> acquireNextImage() override;

    /// Rebuild the swapchain and all dependent resources. This method pauses
    /// execution if the window is minimized and handles the recreation of
    /// views, depth, and color resources after a resize event.
    void recreateSwapchain() override;

    /// Submit the specified 'cmd' command list for execution and present the
    /// swapchain image corresponding to the specified 'imageIndex'. Return
    /// 'true' on success, and 'false' otherwise.
    bool submitAndPresent(CommandListProtocol* cmd,
                          uint32_t             imageIndex) override;

    // ACCESSORS

    /// Return the width of the swapchain images in pixels.
    [[nodiscard]]
    uint32_t width() const override;

    /// Return the height of the swapchain images in pixels.
    [[nodiscard]]
    uint32_t height() const override;

    /// Return the pixel format of the swapchain images.
    [[nodiscard]]
    Format format() const override;

    /// Return the raw Vulkan image handle at the specified 'imageIndex'.
    [[nodiscard]]
    vk::Image image(uint32_t imageIndex) const;

    /// Return the Vulkan image view at the specified 'imageIndex'.
    [[nodiscard]]
    vk::ImageView imageView(uint32_t imageIndex) const;

    /// Return the pixel format of the swapchain images in Vulkan format.
    [[nodiscard]]
    vk::Format vlkFormat() const;

    /// Return the raw Vulkan image handle of the depth buffer.
    [[nodiscard]]
    vk::Image depthImage() const;

    /// Return the raw Vulkan image handle of the color buffer.
    [[nodiscard]]
    vk::Image colorImage() const;

    /// Return the Vulkan image view of the depth buffer.
    [[nodiscard]]
    vk::ImageView depthImageView() const;

    /// Return the Vulkan image view of the color buffer.
    [[nodiscard]]
    vk::ImageView colorImageView() const;

    /// Return the pixel format of the depth buffer.
    [[nodiscard]]
    vk::Format depthFormat() const;
};

// =======================================================================
//                          INLINE DEFINITIONS
// =======================================================================

// ACCESSORS

inline uint32_t Swapchain::width() const
{
    return d_width;
}

inline uint32_t Swapchain::height() const
{
    return d_height;
}

inline vk::Image Swapchain::image(const uint32_t imageIndex) const
{
    return d_images[imageIndex];
}

inline vk::ImageView Swapchain::imageView(const uint32_t imageIndex) const
{
    return *d_imageViews[imageIndex];
}

inline vk::Format Swapchain::vlkFormat() const
{
    return d_format;
}

inline vk::Image Swapchain::depthImage() const
{
    return *d_depthImage;
}

inline vk::Image Swapchain::colorImage() const
{
    return *d_colorImage;
}

inline vk::ImageView Swapchain::depthImageView() const
{
    return *d_depthImageView;
}

inline vk::ImageView Swapchain::colorImageView() const
{
    return *d_colorImageView;
}
inline vk::Format Swapchain::depthFormat() const
{
    return d_depthFormat;
}

}  // close package namespace

#endif  // INCLUDED_ENG_RHI_VLK_SWAPCHAIN_H
