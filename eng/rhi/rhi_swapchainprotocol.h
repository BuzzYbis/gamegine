// rhi_swapchainprotocol.h                                            -*-C++-*-
#ifndef INCLUDED_ENG_RHI_SWAPCHAINPROTOCOL_H
#define INCLUDED_ENG_RHI_SWAPCHAINPROTOCOL_H

//@PURPOSE: Provide a pure abstract interface for the RHI swapchain.
//
//@CLASSES:
//  eng::rhi::SwapchainProtocol: Protocol for window surface image
//  presentation.
//
//@DESCRIPTION: This component provides a pure abstract interface,
// 'eng::rhi::SwapchainProtocol', that represents a swapchain. A swapchain
// manages a queue of images waiting to be presented to the screen, handling
// the synchronization between the CPU, the GPU, and the display hardware.

// std
#include <optional>

namespace eng::rhi {

// Forward declarations
class CommandListProtocol;
enum class Format;

// =======================
// class SwapchainProtocol
// =======================

/// This class provides a protocol (pure abstract interface) representing a
/// swapchain. It is responsible for managing the lifecycle, memory, and
/// presentation of images rendered to a window surface.
class SwapchainProtocol {
  public:
    // CREATORS

    virtual ~SwapchainProtocol() = default;

    // MANIPULATORS

    /// Initialize the underlying RHI swapchain resources. Return 'true' on
    /// success, and 'false' otherwise.
    virtual bool initialize() = 0;

    /// Acquire the next available image from the swapchain for rendering.
    /// Return the index of the acquired image on success, and 'std::nullopt'
    /// if an image cannot be acquired (e.g., if the swapchain is out of date
    /// and needs to be recreated).
    virtual std::optional<uint32_t> acquireNextImage() = 0;

    /// Recreate the swapchain and its associated resources to adapt to new
    /// surface properties (e.g., after a window resize event).
    virtual void recreateSwapchain() = 0;

    /// Submit the specified 'cmd' command list to the graphics queue and
    /// present the swapchain image corresponding to the specified 'imageIndex'
    /// to the screen. Return 'true' on success, and 'false' otherwise.
    virtual bool submitAndPresent(CommandListProtocol* cmd,
                                  uint32_t             imageIndex) = 0;

    // ACCESSORS

    /// Return the current pixel format of the swapchain images.
    [[nodiscard]]
    virtual Format format() const = 0;

    /// Return the current width of the swapchain images in pixels.
    [[nodiscard]]
    virtual uint32_t width() const = 0;

    /// Return the current height of the swapchain images in pixels.
    [[nodiscard]]
    virtual uint32_t height() const = 0;
};

}  // close package namespace
#endif  // INCLUDED_ENG_RHI_SWAPCHAINPROTOCOL_H
