// render_target.h                                                    -*-C++-*-
#ifndef INCLUDED_ENGINE_RHI_VULKAN_VK_RENDER_TARGET_H
#define INCLUDED_ENGINE_RHI_VULKAN_VK_RENDER_TARGET_H

// rhi
#include <rhi/vulkan/vk_context.h>

// vulkan
#include <vulkan/vulkan_raii.hpp>

namespace engine::rhi::vulkan {

class VulkanRenderTarget {
  private:
    // DATA

    VulkanContext& d_context;
    vk::Extent2D   d_extent;
    vk::Format     d_format;

    vk::raii::Image        d_image   = nullptr;
    vk::raii::DeviceMemory d_memory  = nullptr;
    vk::raii::ImageView    d_view    = nullptr;
    vk::raii::Sampler      d_sampler = nullptr;

    vk::raii::Image        d_msaaImage  = nullptr;
    vk::raii::DeviceMemory d_msaaMemory = nullptr;
    vk::raii::ImageView    d_msaaView   = nullptr;

  public:
    // CREATORS

    VulkanRenderTarget(VulkanContext&  context,
                       vk::Extent2D    extent,
                       vk::Format      format,
                       vk::CommandPool commandPool);
    ~VulkanRenderTarget() = default;

    // ACCESSORS

    [[nodiscard]] vk::Image     image() const { return *d_image; }
    [[nodiscard]] vk::ImageView view() const { return *d_view; }
    [[nodiscard]] vk::Sampler   sampler() const { return *d_sampler; }
    [[nodiscard]] vk::ImageView msaaView() const { return *d_msaaView; }
    [[nodiscard]] vk::Extent2D  extent() const { return d_extent; }
    [[nodiscard]] vk::Format    format() const { return d_format; }

  private:
    [[nodiscard]] vk::raii::DeviceMemory
    allocateImageMemory(const vk::raii::Image& image) const;
};

}  // close engine::rhi::vulkan namespace

#endif  // INCLUDED_ENGINE_RHI_VULKAN_VK_RENDER_TARGET_H