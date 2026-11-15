// bench_vulkan.cpp                                                   -*-C++-*-
#include <bench_gpu.h>

// std
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <optional>
#include <string_view>

// third party
#include <fstream>
#include <vulkan/vulkan_raii.hpp>

namespace gamegine {
namespace bench {
namespace {

struct DeviceFeatures {
    bool d_int64BufferAtomics   = false;
    bool d_int64FragmentAtomics = false;
    bool d_meshShaders          = false;
};

struct Candidate {
    vk::raii::PhysicalDevice d_device;
    std::uint32_t            d_queueFamily;
    std::uint32_t            d_timestampValidBits;
    bool                     d_portabilitySubset;
    bool                     d_drawParameters;
    DeviceFeatures           d_features;
    int                      d_rank;
};

struct GpuBuffer {
    vk::raii::DeviceMemory d_memory;
    vk::raii::Buffer       d_buffer;
    std::size_t            d_size;
};

struct GpuTarget {
    vk::raii::DeviceMemory d_memory;
    vk::raii::Image        d_image;
    vk::raii::ImageView    d_view;
    std::uint32_t          d_width;
    std::uint32_t          d_height;
    vk::ImageLayout        d_layout;
};

struct GpuPipeline {
    vk::raii::Pipeline d_pipeline;
    bool               d_compute;
    bool               d_mesh;
};

constexpr auto          k_PORTABILITY_SUBSET = "VK_KHR_portability_subset";
constexpr std::uint32_t k_MAX_BINDINGS       = 8;

bool has_extension(const std::vector<vk::ExtensionProperties>& available,
                   std::string_view                            name)
{
    return std::ranges::any_of(
        available, [name](const vk::ExtensionProperties& e) {
            return std::string_view(e.extensionName.data()) == name;
        });
}

class VulkanGpu final : public Gpu {
    vk::raii::Context             d_context;
    vk::raii::Instance            d_instance;
    vk::raii::PhysicalDevice      d_physicalDevice;
    std::uint32_t                 d_graphicsQueueFamilyIndex;
    vk::raii::Device              d_device;
    vk::raii::CommandPool         d_commandPool;
    vk::raii::DescriptorSetLayout d_descriptorSetLayout;
    vk::raii::PipelineLayout      d_pipelineLayout;
    vk::raii::Queue               d_graphicsQueue;
    vk::raii::DescriptorPool      d_descriptorPool;
    vk::raii::QueryPool           d_queryPool;
    DeviceFeatures                d_features;
    std::vector<GpuBuffer>        d_buffers;
    std::vector<GpuTarget>        d_targets;
    std::vector<GpuPipeline>      d_pipelines;
    float                         d_timestampPeriod;
    std::uint32_t                 d_timestampValidBits;

    [[nodiscard]] Result<std::uint32_t>
    find_memory_type(const vk::MemoryRequirements& memReq,
                     const vk::MemoryPropertyFlags properties) const
    {
        const auto memProps = d_physicalDevice.getMemoryProperties();
        for (std::uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            const bool typeSupported = (memReq.memoryTypeBits & 1u << i) != 0;
            const bool flagsMatch    = (memProps.memoryTypes[i].propertyFlags &
                                     properties) == properties;
            if (typeSupported && flagsMatch) {
                return i;
            }
        }
        return fail("no memory type with {}", vk::to_string(properties));
    }

  public:
    explicit VulkanGpu(vk::raii::Context             context,
                       vk::raii::Instance            instance,
                       vk::raii::PhysicalDevice      physicalDevice,
                       const std::uint32_t           graphicsQueueFamilyIndex,
                       vk::raii::Device              device,
                       vk::raii::CommandPool         commandPool,
                       vk::raii::DescriptorSetLayout descriptorSetLayout,
                       vk::raii::PipelineLayout      pipelineLayout,
                       vk::raii::Queue               graphicsQueue,
                       vk::raii::DescriptorPool      descriptorPool,
                       vk::raii::QueryPool           queryPool,
                       const DeviceFeatures          features,
                       const float                   timestampPeriod,
                       const std::uint32_t           timestampValidBits)
    : d_context(std::move(context))
    , d_instance(std::move(instance))
    , d_physicalDevice(std::move(physicalDevice))
    , d_graphicsQueueFamilyIndex(graphicsQueueFamilyIndex)
    , d_device(std::move(device))
    , d_commandPool(std::move(commandPool))
    , d_descriptorSetLayout(std::move(descriptorSetLayout))
    , d_pipelineLayout(std::move(pipelineLayout))
    , d_graphicsQueue(std::move(graphicsQueue))
    , d_descriptorPool(std::move(descriptorPool))
    , d_queryPool(std::move(queryPool))
    , d_features(features)
    , d_timestampPeriod(timestampPeriod)
    , d_timestampValidBits(timestampValidBits)
    {
    }

    [[nodiscard]] std::string_view api() const override
    {
        return "vulkan";
    }

    [[nodiscard]] std::string_view shader_target() const override
    {
        return "spirv";
    }

    [[nodiscard]] std::vector<std::pair<std::string, std::string>>
    provenance() const override
    {
        const auto chain =
            d_physicalDevice
                .getProperties2<vk::PhysicalDeviceProperties2,
                                vk::PhysicalDeviceDriverProperties>();
        const auto& properties = chain.get<vk::PhysicalDeviceProperties2>()
                                     .properties;
        const auto& driver = chain.get<vk::PhysicalDeviceDriverProperties>();
        const auto& conformance = driver.conformanceVersion;
        const auto  family =
            d_physicalDevice
                .getQueueFamilyProperties()[d_graphicsQueueFamilyIndex];

        return {
            {"api", "vulkan"},
            {"device_name", properties.deviceName.data()},
            {"device_type", vk::to_string(properties.deviceType)},
            {"vendor_id", std::format("{:#06x}", properties.vendorID)},
            {"device_id", std::format("{:#06x}", properties.deviceID)},
            {
                "api_version",
                std::format("{}.{}.{}",
                            VK_API_VERSION_MAJOR(properties.apiVersion),
                            VK_API_VERSION_MINOR(properties.apiVersion),
                            VK_API_VERSION_PATCH(properties.apiVersion)),
            },
            {"driver", vk::to_string(driver.driverID)},
            {"driver_name", driver.driverName.data()},
            {"driver_info", driver.driverInfo.data()},
            {
                "driver_version",
                std::format("{:#010x}", properties.driverVersion),
            },
            {
                "conformance_version",
                std::format("{}.{}.{}.{}",
                            conformance.major,
                            conformance.minor,
                            conformance.subminor,
                            conformance.patch),
            },
            {
                "timestamp_period_ns",
                std::format("{}", properties.limits.timestampPeriod),
            },
            {
                "timestamp_valid_bits",
                std::to_string(family.timestampValidBits),
            },
            {
                "int64_buffer_atomics",
                d_features.d_int64BufferAtomics ? "yes" : "no",
            },
            {
                "int64_fragment_atomics",
                d_features.d_int64FragmentAtomics ? "yes" : "no",
            },
            {"mesh_shaders", d_features.d_meshShaders ? "yes" : "no"},
        };
    }

    [[nodiscard]] bool supports(const Feature feature) const override
    {
        switch (feature) {
        case Feature::e_INT64_BUFFER_ATOMICS:
            return d_features.d_int64BufferAtomics;
        case Feature::e_INT64_FRAGMENT_ATOMICS:
            return d_features.d_int64FragmentAtomics;
        case Feature::e_MESH_SHADERS: return d_features.d_meshShaders;
        }
        return false;
    }

    Result<Buffer> create_buffer(std::size_t bytes) override
    {
        try {
            vk::BufferCreateInfo bufferInfo{};
            bufferInfo.size  = std::max<std::size_t>(bytes, 1);
            bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer |
                               vk::BufferUsageFlagBits::eTransferDst |
                               vk::BufferUsageFlagBits::eTransferSrc;
            auto buffer = vk::raii::Buffer(d_device, bufferInfo);
            const vk::MemoryRequirements memReq = buffer
                                                      .getMemoryRequirements();
            vk::MemoryAllocateInfo allocInfo{};
            allocInfo.allocationSize = memReq.size;
            constexpr auto properties =
                vk::MemoryPropertyFlagBits::eDeviceLocal;

            const auto memoryTypeIndex = find_memory_type(memReq, properties);

            if (!memoryTypeIndex) {
                return fail("no device-local memory type for a {}-byte buffer",
                            bytes);
            }

            allocInfo.memoryTypeIndex = *memoryTypeIndex;

            auto deviceMemory = vk::raii::DeviceMemory(d_device, allocInfo);
            buffer.bindMemory(*deviceMemory, 0);
            d_buffers.push_back({
                .d_memory = std::move(deviceMemory),
                .d_buffer = std::move(buffer),
                .d_size   = bytes,
            });
        } catch (const std::exception& e) {
            return fail("cannot allocate a {}-byte buffer: {}",
                        bytes,
                        e.what());
        }
        return Buffer{static_cast<std::uint32_t>(d_buffers.size() - 1)};
    }

    Result<void> upload(Buffer to, std::span<const std::byte> data) override
    {
        if (data.empty()) {
            return {};
        }

        if (to.d_id >= d_buffers.size()) {
            return fail("upload into unknown buffer {}", to.d_id);
        }

        if (data.size() > d_buffers[to.d_id].d_size) {
            return fail("upload of {} bytes into a {}-byte buffer",
                        data.size(),
                        d_buffers[to.d_id].d_size);
        }

        try {
            vk::BufferCreateInfo bufferInfo{};
            bufferInfo.size  = data.size();
            bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
            auto buffer      = vk::raii::Buffer(d_device, bufferInfo);
            const vk::MemoryRequirements memReq = buffer
                                                      .getMemoryRequirements();
            vk::MemoryAllocateInfo allocInfo{};
            allocInfo.allocationSize = memReq.size;

            constexpr auto properties =
                vk::MemoryPropertyFlagBits::eHostVisible |
                vk::MemoryPropertyFlagBits::eHostCoherent;

            const auto memoryTypeIndex = find_memory_type(memReq, properties);

            if (!memoryTypeIndex) {
                return fail("no host-visible memory type for a {}-byte buffer",
                            data.size());
            }
            allocInfo.memoryTypeIndex = *memoryTypeIndex;

            auto deviceMemory = vk::raii::DeviceMemory(d_device, allocInfo);
            buffer.bindMemory(*deviceMemory, 0);
            void* mapped = deviceMemory.mapMemory(0, data.size());
            std::memcpy(mapped, data.data(), data.size());
            deviceMemory.unmapMemory();

            vk::CommandBufferAllocateInfo cmdAlloc{};
            cmdAlloc.commandPool        = d_commandPool;
            cmdAlloc.level              = vk::CommandBufferLevel::ePrimary;
            cmdAlloc.commandBufferCount = 1;
            const vk::raii::CommandBuffer cmd = std::move(
                vk::raii::CommandBuffers(d_device, cmdAlloc).front());

            cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

            constexpr vk::MemoryBarrier2
                barrier{vk::PipelineStageFlagBits2::eAllCommands,
                        vk::AccessFlagBits2::eMemoryWrite,
                        vk::PipelineStageFlagBits2::eAllCommands,
                        vk::AccessFlagBits2::eMemoryRead |
                            vk::AccessFlagBits2::eMemoryWrite};
            cmd.pipelineBarrier2(
                vk::DependencyInfo{}.setMemoryBarriers(barrier));
            cmd.copyBuffer(*buffer,
                           *d_buffers[to.d_id].d_buffer,
                           vk::BufferCopy{0, 0, data.size()});
            cmd.end();

            const vk::raii::Fence fence(d_device, vk::FenceCreateInfo{});
            vk::SubmitInfo        submitInfo{};
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers    = &*cmd;
            d_graphicsQueue.submit(submitInfo, *fence);
            const vk::Result result = d_device.waitForFences(*fence,
                                                             VK_TRUE,
                                                             UINT64_MAX);

            if (result != vk::Result::eSuccess) {
                return fail("waiting for the upload fence failed: {}",
                            vk::to_string(result));
            }

        } catch (const std::exception& e) {
            return fail("cannot upload a {}-byte buffer: {}",
                        data.size(),
                        e.what());
        }
        return {};
    }

    Result<void> download(Buffer from, std::span<std::byte> data) override
    {
        if (data.empty()) {
            return {};
        }

        if (from.d_id >= d_buffers.size()) {
            return fail("download from unknown buffer {}", from.d_id);
        }

        if (data.size() > d_buffers[from.d_id].d_size) {
            return fail("download of {} bytes from a {}-byte buffer",
                        data.size(),
                        d_buffers[from.d_id].d_size);
        }

        try {
            vk::BufferCreateInfo bufferInfo{};
            bufferInfo.size  = data.size();
            bufferInfo.usage = vk::BufferUsageFlagBits::eTransferDst;
            auto buffer      = vk::raii::Buffer(d_device, bufferInfo);

            const vk::MemoryRequirements memReq = buffer
                                                      .getMemoryRequirements();
            vk::MemoryAllocateInfo allocInfo{};
            allocInfo.allocationSize = memReq.size;

            constexpr auto properties =
                vk::MemoryPropertyFlagBits::eHostVisible |
                vk::MemoryPropertyFlagBits::eHostCoherent;

            const auto memoryTypeIndex = find_memory_type(memReq, properties);

            if (!memoryTypeIndex) {
                return fail("no host-visible memory type for a {}-byte buffer",
                            data.size());
            }
            allocInfo.memoryTypeIndex = *memoryTypeIndex;

            auto deviceMemory = vk::raii::DeviceMemory(d_device, allocInfo);
            buffer.bindMemory(*deviceMemory, 0);

            vk::CommandBufferAllocateInfo cmdAlloc{};
            cmdAlloc.commandPool        = d_commandPool;
            cmdAlloc.level              = vk::CommandBufferLevel::ePrimary;
            cmdAlloc.commandBufferCount = 1;
            const vk::raii::CommandBuffer cmd = std::move(
                vk::raii::CommandBuffers(d_device, cmdAlloc).front());

            cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

            constexpr vk::MemoryBarrier2
                barrier{vk::PipelineStageFlagBits2::eAllCommands,
                        vk::AccessFlagBits2::eMemoryWrite,
                        vk::PipelineStageFlagBits2::eAllCommands,
                        vk::AccessFlagBits2::eMemoryRead |
                            vk::AccessFlagBits2::eMemoryWrite};
            cmd.pipelineBarrier2(
                vk::DependencyInfo{}.setMemoryBarriers(barrier));
            cmd.copyBuffer(*d_buffers[from.d_id].d_buffer,
                           *buffer,
                           vk::BufferCopy{0, 0, data.size()});
            cmd.end();

            const vk::raii::Fence fence(d_device, vk::FenceCreateInfo{});
            vk::SubmitInfo        submitInfo{};
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers    = &*cmd;
            d_graphicsQueue.submit(submitInfo, *fence);
            const vk::Result result = d_device.waitForFences(*fence,
                                                             VK_TRUE,
                                                             UINT64_MAX);

            if (result != vk::Result::eSuccess) {
                return fail("waiting for the download fence failed: {}",
                            vk::to_string(result));
            }

            const void* mapped = deviceMemory.mapMemory(0, data.size());
            std::memcpy(data.data(), mapped, data.size());
            deviceMemory.unmapMemory();
        } catch (const std::exception& e) {
            return fail("cannot download a {}-byte buffer: {}",
                        data.size(),
                        e.what());
        }
        return {};
    }

    Result<Target> create_target(std::uint32_t width,
                                 std::uint32_t height) override
    {
        const std::uint32_t max = d_physicalDevice.getProperties()
                                      .limits.maxImageDimension2D;
        if (width == 0 || height == 0 || width > max || height > max) {
            return fail("cannot create a {}x{} render target", width, height);
        }

        try {
            vk::ImageCreateInfo imageInfo{};
            imageInfo.imageType     = vk::ImageType::e2D;
            imageInfo.extent.width  = width;
            imageInfo.extent.height = height;
            imageInfo.extent.depth  = 1;
            imageInfo.mipLevels     = 1;
            imageInfo.arrayLayers   = 1;
            imageInfo.format        = vk::Format::eR8G8B8A8Unorm;
            imageInfo.tiling        = vk::ImageTiling::eOptimal;
            imageInfo.initialLayout = vk::ImageLayout::eUndefined;
            imageInfo.usage = vk::ImageUsageFlagBits::eColorAttachment |
                              vk::ImageUsageFlagBits::eTransferSrc;
            imageInfo.samples     = vk::SampleCountFlagBits::e1;
            imageInfo.sharingMode = vk::SharingMode::eExclusive;

            auto image = vk::raii::Image(d_device, imageInfo);

            const vk::MemoryRequirements memReq = image
                                                      .getMemoryRequirements();
            constexpr auto properties =
                vk::MemoryPropertyFlagBits::eDeviceLocal;
            vk::MemoryAllocateInfo allocInfo{};
            allocInfo.allocationSize = memReq.size;

            const auto memoryTypeIndex = find_memory_type(memReq, properties);

            if (!memoryTypeIndex) {
                return fail(
                    "no device-local memory type for a {}x{} render target",
                    width,
                    height);
            }
            allocInfo.memoryTypeIndex = *memoryTypeIndex;
            auto imageMemory = vk::raii::DeviceMemory(d_device, allocInfo);
            image.bindMemory(*imageMemory, 0);

            vk::ImageViewCreateInfo viewInfo{};
            viewInfo.image    = *image;
            viewInfo.viewType = vk::ImageViewType::e2D;
            viewInfo.format   = imageInfo.format;
            viewInfo.subresourceRange.aspectMask =
                vk::ImageAspectFlagBits::eColor;
            viewInfo.subresourceRange.baseMipLevel   = 0;
            viewInfo.subresourceRange.levelCount     = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount     = 1;

            auto imageView = vk::raii::ImageView(d_device, viewInfo);

            d_targets.push_back({
                .d_memory = std::move(imageMemory),
                .d_image  = std::move(image),
                .d_view   = std::move(imageView),
                .d_width  = width,
                .d_height = height,
                .d_layout = vk::ImageLayout::eUndefined,
            });

        } catch (const std::exception& e) {
            return fail("error while creating a {}x{} render target: {}",
                        width,
                        height,
                        e.what());
        }

        return Target{static_cast<std::uint32_t>(d_targets.size() - 1)};
    }

    Result<void> read_target(Target                     from,
                             const std::span<std::byte> rgba8) override
    {
        if (from.d_id >= d_targets.size()) {
            return fail("read from unknown target {}", from.d_id);
        }

        GpuTarget& target = d_targets[from.d_id];

        const std::size_t bytes = std::size_t{target.d_width} *
                                  target.d_height * 4;
        if (rgba8.size() != bytes) {
            return fail("read of a {}x{} target into {} bytes, not {}",
                        target.d_width,
                        target.d_height,
                        rgba8.size(),
                        bytes);
        }

        try {
            vk::BufferCreateInfo bufferInfo{};
            bufferInfo.size  = rgba8.size();
            bufferInfo.usage = vk::BufferUsageFlagBits::eTransferDst;
            auto buffer      = vk::raii::Buffer(d_device, bufferInfo);

            const vk::MemoryRequirements memReq = buffer
                                                      .getMemoryRequirements();
            vk::MemoryAllocateInfo allocInfo{};
            allocInfo.allocationSize = memReq.size;

            constexpr auto properties =
                vk::MemoryPropertyFlagBits::eHostVisible |
                vk::MemoryPropertyFlagBits::eHostCoherent;

            const auto memoryTypeIndex = find_memory_type(memReq, properties);

            if (!memoryTypeIndex) {
                return fail("no host-visible memory type for a {}-byte buffer",
                            rgba8.size());
            }
            allocInfo.memoryTypeIndex = *memoryTypeIndex;

            auto deviceMemory = vk::raii::DeviceMemory(d_device, allocInfo);
            buffer.bindMemory(*deviceMemory, 0);

            vk::CommandBufferAllocateInfo cmdAlloc{};
            cmdAlloc.commandPool        = d_commandPool;
            cmdAlloc.level              = vk::CommandBufferLevel::ePrimary;
            cmdAlloc.commandBufferCount = 1;
            const vk::raii::CommandBuffer cmd = std::move(
                vk::raii::CommandBuffers(d_device, cmdAlloc).front());

            cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

            vk::ImageMemoryBarrier2 barrier{};
            barrier.srcStageMask  = vk::PipelineStageFlagBits2::eAllCommands;
            barrier.srcAccessMask = vk::AccessFlagBits2::eMemoryWrite;
            barrier.dstStageMask  = vk::PipelineStageFlagBits2::eCopy;
            barrier.dstAccessMask = vk::AccessFlagBits2::eTransferRead;
            barrier.oldLayout     = target.d_layout;
            barrier.newLayout     = vk::ImageLayout::eTransferSrcOptimal;
            barrier.image         = *target.d_image;
            barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor,
                                        0,
                                        1,
                                        0,
                                        1};
            cmd.pipelineBarrier2(
                vk::DependencyInfo{}.setImageMemoryBarriers(barrier));

            vk::BufferImageCopy region{};
            region.bufferOffset      = 0;
            region.bufferRowLength   = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource  = {vk::ImageAspectFlagBits::eColor,
                                        0,
                                        0,
                                        1};
            region.imageOffset       = vk::Offset3D{0, 0, 0};
            region.imageExtent       = vk::Extent3D{target.d_width,
                                              target.d_height,
                                              1};
            cmd.copyImageToBuffer(*target.d_image,
                                  vk::ImageLayout::eTransferSrcOptimal,
                                  *buffer,
                                  region);
            cmd.end();

            const vk::raii::Fence fence(d_device, vk::FenceCreateInfo{});
            vk::SubmitInfo        submitInfo{};
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers    = &*cmd;
            d_graphicsQueue.submit(submitInfo, *fence);
            const vk::Result result = d_device.waitForFences(*fence,
                                                             VK_TRUE,
                                                             UINT64_MAX);

            if (result != vk::Result::eSuccess) {
                return fail("waiting for the read fence failed: {}",
                            vk::to_string(result));
            }

            target.d_layout    = vk::ImageLayout::eTransferSrcOptimal;
            const void* mapped = deviceMemory.mapMemory(0, rgba8.size());
            std::memcpy(rgba8.data(), mapped, rgba8.size());
            deviceMemory.unmapMemory();
        } catch (const std::exception& e) {
            return fail("cannot read a {}x{} target: {}",
                        target.d_width,
                        target.d_height,
                        e.what());
        }

        return {};
    }

    Result<Pipeline> create_pipeline(const PipelineDesc& desc) override
    {
        if (desc.d_mesh && desc.d_fragment.empty()) {
            return fail("mesh pipeline '{}' has no fragment entry point",
                        desc.d_entry);
        }

        std::ifstream file(desc.d_artifact, std::ios::ate | std::ios::binary);
        if (!file) {
            return fail("cannot open {}", desc.d_artifact.string());
        }

        const std::streamoff size = file.tellg();
        if (size <= 0 || size % 4 != 0) {
            return fail("cannot open {}", desc.d_artifact.string());
        }

        std::vector<std::uint32_t> code(static_cast<std::size_t>(size) / 4);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(code.data()), size);
        if (!file) {
            return fail("cannot open {}", desc.d_artifact.string());
        }

        try {
            const vk::ShaderModuleCreateInfo moduleInfo(
                {}, static_cast<std::size_t>(size), code.data());
            const vk::raii::ShaderModule module(d_device, moduleInfo);

            if (desc.d_fragment.empty()) {
                vk::ComputePipelineCreateInfo info{};
                info.stage = vk::PipelineShaderStageCreateInfo(
                    {},
                    vk::ShaderStageFlagBits::eCompute,
                    *module,
                    desc.d_entry.c_str());
                info.layout = *d_pipelineLayout;

                d_pipelines.push_back({
                    .d_pipeline = vk::raii::Pipeline(d_device, nullptr, info),
                    .d_compute  = true,
                    .d_mesh     = false,
                });
            } else {
                const std::array stages = {
                    vk::PipelineShaderStageCreateInfo(
                        {},
                        desc.d_mesh ? vk::ShaderStageFlagBits::eMeshEXT
                                    : vk::ShaderStageFlagBits::eVertex,
                        *module,
                        desc.d_entry.c_str()),
                    vk::PipelineShaderStageCreateInfo(
                        {},
                        vk::ShaderStageFlagBits::eFragment,
                        *module,
                        desc.d_fragment.c_str()),
                };
                constexpr vk::PipelineVertexInputStateCreateInfo vertexInput{};
                constexpr vk::PipelineInputAssemblyStateCreateInfo
                    inputAssembly({}, vk::PrimitiveTopology::eTriangleList);

                constexpr vk::PipelineViewportStateCreateInfo viewport(
                    {}, 1, nullptr, 1, nullptr);
                constexpr std::array dynamicStates =
                    {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
                const vk::PipelineDynamicStateCreateInfo dynamic(
                    {}, dynamicStates);
                vk::PipelineRasterizationStateCreateInfo raster{};
                raster.polygonMode = vk::PolygonMode::eFill;
                raster.cullMode    = desc.d_cullBack
                                         ? vk::CullModeFlagBits::eBack
                                         : vk::CullModeFlagBits::eNone;
                raster.frontFace   = vk::FrontFace::eCounterClockwise;
                raster.lineWidth   = 1.0f;

                constexpr vk::PipelineMultisampleStateCreateInfo multisample(
                    {}, vk::SampleCountFlagBits::e1);

                vk::PipelineColorBlendAttachmentState attachment{};
                attachment.colorWriteMask = vk::ColorComponentFlagBits::eR |
                                            vk::ColorComponentFlagBits::eG |
                                            vk::ColorComponentFlagBits::eB |
                                            vk::ColorComponentFlagBits::eA;
                const vk::PipelineColorBlendStateCreateInfo blend(
                    {}, VK_FALSE, vk::LogicOp::eCopy, attachment);

                constexpr auto format = vk::Format::eR8G8B8A8Unorm;
                vk::PipelineRenderingCreateInfo rendering{};
                rendering.setColorAttachmentFormats(format);
                vk::GraphicsPipelineCreateInfo info{};
                info.pNext = &rendering;
                info.setStages(stages);
                info.pVertexInputState = desc.d_mesh ? nullptr : &vertexInput;
                info.pInputAssemblyState = desc.d_mesh ? nullptr
                                                       : &inputAssembly;
                info.pViewportState      = &viewport;
                info.pRasterizationState = &raster;
                info.pMultisampleState   = &multisample;
                info.pColorBlendState    = &blend;
                info.pDynamicState       = &dynamic;
                info.layout              = *d_pipelineLayout;
                d_pipelines.push_back({
                    .d_pipeline = vk::raii::Pipeline(d_device, nullptr, info),
                    .d_compute  = false,
                    .d_mesh     = desc.d_mesh,
                });
            }
        } catch (const std::exception& e) {
            return fail("cannot create the '{}' pipeline: {}",
                        desc.d_entry,
                        e.what());
        }
        return Pipeline{static_cast<std::uint32_t>(d_pipelines.size() - 1)};
    }

    Result<Timing> run(std::span<const Command> commands) override
    {
        if (commands.empty()) {
            return fail("an empty run has nothing to time");
        }

        Timing timing;
        try {
            vk::CommandBufferAllocateInfo cmdAlloc{};
            cmdAlloc.commandPool        = d_commandPool;
            cmdAlloc.level              = vk::CommandBufferLevel::ePrimary;
            cmdAlloc.commandBufferCount = 1;
            const vk::raii::CommandBuffer cmd = std::move(
                vk::raii::CommandBuffers(d_device, cmdAlloc).front());

            cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
            cmd.resetQueryPool(*d_queryPool, 0, 2);
            cmd.writeTimestamp2(vk::PipelineStageFlagBits2::eTopOfPipe,
                                *d_queryPool,
                                0);

            std::vector<vk::raii::DescriptorSet> sets;
            std::vector<std::uint32_t>           drawn;
            const vk::DescriptorSetLayout setLayout = *d_descriptorSetLayout;

            for (std::size_t i = 0; i < commands.size(); ++i) {
                const Command& command  = commands[i];
                const auto*    dispatch = std::get_if<Dispatch>(&command);
                const auto*    draw     = std::get_if<Draw>(&command);
                const auto*    mesh     = std::get_if<MeshDraw>(&command);
                const Pipeline id       = std::visit(
                    [](const auto& c) { return c.d_pipeline; }, command);
                // Through a pointer: GCC 13 takes a reference returned by
                // std::visit for one to its temporary visitor.
                const std::vector<Binding>& bindings = *std::visit(
                    [](const auto& c) { return &c.d_bindings; }, command);

                if (id.d_id >= d_pipelines.size()) {
                    return fail("command {}: unknown pipeline {}", i, id.d_id);
                }
                const GpuPipeline& pipeline = d_pipelines[id.d_id];
                if (pipeline.d_compute != (dispatch != nullptr) ||
                    (!pipeline.d_compute &&
                     pipeline.d_mesh != (mesh != nullptr))) {
                    return fail("command {}: pipeline {} is not a {} "
                                "pipeline",
                                i,
                                id.d_id,
                                dispatch != nullptr ? "compute"
                                : mesh != nullptr   ? "mesh"
                                                    : "vertex");
                }
                for (const Binding& b : bindings) {
                    if (b.d_buffer.d_id >= d_buffers.size()) {
                        return fail("command {}: binding {} names unknown "
                                    "buffer {}",
                                    i,
                                    b.d_index,
                                    b.d_buffer.d_id);
                    }
                    if (b.d_index >= k_MAX_BINDINGS) {
                        return fail("command {}: binding {} is past the {} "
                                    "the layout has",
                                    i,
                                    b.d_index,
                                    k_MAX_BINDINGS);
                    }
                }
                const GpuTarget* target   = nullptr;
                std::uint32_t    targetId = 0;
                if (dispatch == nullptr) {
                    const Target t = draw != nullptr ? draw->d_target
                                                     : mesh->d_target;
                    if (t.d_id >= d_targets.size()) {
                        return fail("command {}: unknown target {}",
                                    i,
                                    t.d_id);
                    }
                    target   = &d_targets[t.d_id];
                    targetId = t.d_id;
                }

                constexpr vk::MemoryBarrier2
                    barrier{vk::PipelineStageFlagBits2::eAllCommands,
                            vk::AccessFlagBits2::eMemoryWrite,
                            vk::PipelineStageFlagBits2::eAllCommands,
                            vk::AccessFlagBits2::eMemoryRead |
                                vk::AccessFlagBits2::eMemoryWrite};
                vk::DependencyInfo dependency{};
                dependency.setMemoryBarriers(barrier);
                vk::ImageMemoryBarrier2 toAttachment{};
                if (target != nullptr) {
                    toAttachment.srcStageMask =
                        vk::PipelineStageFlagBits2::eAllCommands;
                    toAttachment.srcAccessMask =
                        vk::AccessFlagBits2::eMemoryWrite;
                    toAttachment.dstStageMask =
                        vk::PipelineStageFlagBits2::eColorAttachmentOutput;
                    toAttachment.dstAccessMask =
                        vk::AccessFlagBits2::eColorAttachmentWrite;
                    toAttachment.oldLayout = vk::ImageLayout::eUndefined;
                    toAttachment.newLayout =
                        vk::ImageLayout::eColorAttachmentOptimal;
                    toAttachment.image = *target->d_image;
                    toAttachment.subresourceRange =
                        {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
                    dependency.setImageMemoryBarriers(toAttachment);
                }
                cmd.pipelineBarrier2(dependency);

                vk::DescriptorSetAllocateInfo setInfo{};
                setInfo.descriptorPool = *d_descriptorPool;
                setInfo.setSetLayouts(setLayout);
                sets.push_back(std::move(
                    vk::raii::DescriptorSets(d_device, setInfo).front()));
                const vk::DescriptorSet set = *sets.back();

                std::vector<vk::DescriptorBufferInfo> infos;
                infos.reserve(bindings.size());
                for (const Binding& b : bindings) {
                    infos.emplace_back(*d_buffers[b.d_buffer.d_id].d_buffer,
                                       0,
                                       VK_WHOLE_SIZE);
                }
                std::vector<vk::WriteDescriptorSet> writes;
                writes.reserve(bindings.size());
                for (std::size_t b = 0; b < bindings.size(); ++b) {
                    writes.emplace_back(set,
                                        bindings[b].d_index,
                                        0,
                                        1,
                                        vk::DescriptorType::eStorageBuffer,
                                        nullptr,
                                        &infos[b]);
                }
                d_device.updateDescriptorSets(writes, {});

                if (dispatch != nullptr) {
                    cmd.bindPipeline(vk::PipelineBindPoint::eCompute,
                                     *pipeline.d_pipeline);
                    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                           *d_pipelineLayout,
                                           0,
                                           set,
                                           {});
                    const auto& [x, y, z] = dispatch->d_groups;
                    cmd.dispatch(x, y, z);
                    continue;
                }

                const vk::Extent2D extent{target->d_width, target->d_height};
                vk::RenderingAttachmentInfo colour{};
                colour.imageView   = *target->d_view;
                colour.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
                colour.loadOp      = vk::AttachmentLoadOp::eClear;
                colour.storeOp     = vk::AttachmentStoreOp::eStore;
                colour.clearValue  = vk::ClearColorValue(
                    std::array{0.0f, 0.0f, 0.0f, 0.0f});
                vk::RenderingInfo rendering{};
                rendering.renderArea = vk::Rect2D({0, 0}, extent);
                rendering.layerCount = 1;
                rendering.setColorAttachments(colour);

                cmd.beginRendering(rendering);
                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                 *pipeline.d_pipeline);
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                       *d_pipelineLayout,
                                       0,
                                       set,
                                       {});
                const auto width  = static_cast<float>(extent.width);
                const auto height = static_cast<float>(extent.height);
                cmd.setViewport(
                    0, vk::Viewport(0.0f, height, width, -height, 0.0f, 1.0f));
                cmd.setScissor(0, vk::Rect2D({0, 0}, extent));
                if (draw != nullptr) {
                    cmd.draw(draw->d_vertices, draw->d_instances, 0, 0);
                } else {
                    const auto& [x, y, z] = mesh->d_groups;
                    cmd.drawMeshTasksEXT(x, y, z);
                }
                cmd.endRendering();
                drawn.push_back(targetId);
            }

            cmd.writeTimestamp2(vk::PipelineStageFlagBits2::eBottomOfPipe,
                                *d_queryPool,
                                1);
            cmd.end();

            const vk::raii::Fence fence(d_device, vk::FenceCreateInfo{});
            vk::SubmitInfo        submitInfo{};
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers    = &*cmd;
            d_graphicsQueue.submit(submitInfo, *fence);

            const auto       submitted = std::chrono::steady_clock::now();
            const vk::Result waited    = d_device.waitForFences(*fence,
                                                             VK_TRUE,
                                                             UINT64_MAX);
            const auto       done      = std::chrono::steady_clock::now();
            if (waited != vk::Result::eSuccess) {
                return fail("waiting for the run fence failed: {}",
                            vk::to_string(waited));
            }
            for (const std::uint32_t t : drawn) {
                d_targets[t].d_layout =
                    vk::ImageLayout::eColorAttachmentOptimal;
            }

            const auto [read, ticks] = d_queryPool.getResults<std::uint64_t>(
                0,
                2,
                2 * sizeof(std::uint64_t),
                sizeof(std::uint64_t),
                vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);
            if (read != vk::Result::eSuccess) {
                return fail("the GPU timestamps could not be read: {}",
                            vk::to_string(read));
            }

            const std::uint64_t mask    = d_timestampValidBits >= 64
                                              ? ~std::uint64_t{0}
                                              : (std::uint64_t{1}
                                              << d_timestampValidBits) -
                                                 1;
            const std::uint64_t elapsed = (ticks[1] - ticks[0]) & mask;
            timing.d_gpuNs              = static_cast<double>(elapsed) *
                             static_cast<double>(d_timestampPeriod);
            timing.d_independentNs = std::chrono::duration<double, std::nano>(
                                         done - submitted)
                                         .count();
        } catch (const std::exception& e) {
            return fail("cannot run the commands: {}", e.what());
        }
        return timing;
    }
};

}  // close unnamed namespace

Result<std::unique_ptr<Gpu>> create_vulkan()
{
    vk::raii::Context  context;
    vk::raii::Instance instance = nullptr;
    try {
        vk::ApplicationInfo appInfo{};
        appInfo.pApplicationName   = "Bench";
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.pEngineName        = "vg_bench";
        appInfo.engineVersion      = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.apiVersion         = vk::ApiVersion13;

        std::vector<const char*> layers;
        std::vector<const char*> extensions;

#ifdef __APPLE__
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

        for (const auto& required : extensions) {
            const bool found = std::ranges::any_of(
                context.enumerateInstanceExtensionProperties(),
                [required](const auto& prop) {
                    return strcmp(prop.extensionName, required) == 0;
                });

            if (!found) {
                return fail("Failed to find all required extensions.");
            }
        }

        vk::InstanceCreateInfo createInfo{};
        createInfo.pApplicationInfo  = &appInfo;
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(
            layers.size());
        createInfo.ppEnabledLayerNames   = layers.data();
        createInfo.enabledExtensionCount = static_cast<std::uint32_t>(
            extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

#ifdef __APPLE__
        createInfo.flags =
            vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif

        instance = vk::raii::Instance(context, createInfo);
    } catch (const std::exception& e) {
        return fail("Failed to create Vulkan Instance: {}", e.what());
    }

    std::optional<Candidate> choice;
    std::string              rejected;
    for (const vk::raii::PhysicalDevice& physical :
         instance.enumeratePhysicalDevices()) {
        const vk::PhysicalDeviceProperties properties = physical
                                                            .getProperties();
        const std::string_view name = properties.deviceName.data();
        if (properties.apiVersion < vk::ApiVersion13) {
            rejected += std::format(
                "\n  {}: Vulkan {}.{}, the bench needs 1.3",
                name,
                VK_API_VERSION_MAJOR(properties.apiVersion),
                VK_API_VERSION_MINOR(properties.apiVersion));
            continue;
        }

        const auto families             = physical.getQueueFamilyProperties();
        constexpr vk::QueueFlags needed = vk::QueueFlagBits::eGraphics |
                                          vk::QueueFlagBits::eCompute;
        std::optional<std::uint32_t> family;
        for (std::uint32_t i = 0; i < families.size() && !family; ++i) {
            if ((families[i].queueFlags & needed) == needed &&
                families[i].timestampValidBits > 0) {
                family = i;
            }
        }
        if (!family) {
            rejected += std::format("\n  {}: no queue family with graphics, "
                                    "compute and timestamps",
                                    name);
            continue;
        }

        const auto chain =
            physical.getFeatures2<vk::PhysicalDeviceFeatures2,
                                  vk::PhysicalDeviceVulkan11Features,
                                  vk::PhysicalDeviceVulkan12Features,
                                  vk::PhysicalDeviceVulkan13Features>();
        const auto& core = chain.get<vk::PhysicalDeviceFeatures2>().features;
        const auto& v11  = chain.get<vk::PhysicalDeviceVulkan11Features>();
        const auto& v12  = chain.get<vk::PhysicalDeviceVulkan12Features>();
        const auto& v13  = chain.get<vk::PhysicalDeviceVulkan13Features>();
        if (v13.dynamicRendering != VK_TRUE ||
            v13.synchronization2 != VK_TRUE) {
            rejected += std::format("\n  {}: no dynamicRendering or "
                                    "synchronization2",
                                    name);
            continue;
        }

        const auto extensions = physical.enumerateDeviceExtensionProperties();
        const bool mesh =
            has_extension(extensions, vk::EXTMeshShaderExtensionName) &&
            physical.getFeatures2<vk::PhysicalDeviceFeatures2,
                                  vk::PhysicalDeviceMeshShaderFeaturesEXT>()
                    .get<vk::PhysicalDeviceMeshShaderFeaturesEXT>()
                    .meshShader == VK_TRUE;

        int rank = 0;
        switch (properties.deviceType) {
        case vk::PhysicalDeviceType::eDiscreteGpu: rank = 3; break;
        case vk::PhysicalDeviceType::eIntegratedGpu: rank = 2; break;
        case vk::PhysicalDeviceType::eVirtualGpu: rank = 1; break;
        default: break;
        }
        if (choice && rank <= choice->d_rank) {
            continue;
        }

        Candidate c{
            .d_device             = physical,
            .d_queueFamily        = *family,
            .d_timestampValidBits = families[*family].timestampValidBits,
            .d_portabilitySubset  = has_extension(extensions,
                                                 k_PORTABILITY_SUBSET),
            .d_drawParameters     = v11.shaderDrawParameters == VK_TRUE,
            .d_features           = {},
            .d_rank               = rank,
        };
        c.d_features.d_int64BufferAtomics = core.shaderInt64 == VK_TRUE &&
                                            v12.shaderBufferInt64Atomics ==
                                                VK_TRUE;
        c.d_features.d_int64FragmentAtomics =
            c.d_features.d_int64BufferAtomics &&
            core.fragmentStoresAndAtomics == VK_TRUE && c.d_drawParameters;
        c.d_features.d_meshShaders = mesh;
        choice                     = std::move(c);
    }
    if (!choice) {
        return fail("no Vulkan device can run the bench:{}",
                    rejected.empty() ? std::string(" none found") : rejected);
    }
    const DeviceFeatures& features = choice->d_features;

    std::vector<const char*> deviceExtensions;
    if (choice->d_portabilitySubset) {
        deviceExtensions.push_back(k_PORTABILITY_SUBSET);
    }
    if (features.d_meshShaders) {
        deviceExtensions.push_back(vk::EXTMeshShaderExtensionName);
    }

    vk::raii::Device              device              = nullptr;
    vk::raii::Queue               graphicsQueue       = nullptr;
    vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
    vk::raii::PipelineLayout      pipelineLayout      = nullptr;
    vk::raii::CommandPool         commandPool         = nullptr;
    vk::raii::DescriptorPool      descriptorPool      = nullptr;
    vk::raii::QueryPool           queryPool           = nullptr;

    try {
        constexpr float                 queuePriority = 1.0f;
        const vk::DeviceQueueCreateInfo queueCreateInfo({},
                                                        choice->d_queueFamily,
                                                        1,
                                                        &queuePriority);

        vk::StructureChain<vk::DeviceCreateInfo,
                           vk::PhysicalDeviceFeatures2,
                           vk::PhysicalDeviceVulkan11Features,
                           vk::PhysicalDeviceVulkan12Features,
                           vk::PhysicalDeviceVulkan13Features,
                           vk::PhysicalDeviceMeshShaderFeaturesEXT>
              chain;
        auto& core       = chain.get<vk::PhysicalDeviceFeatures2>().features;
        core.shaderInt64 = features.d_int64BufferAtomics ? VK_TRUE : VK_FALSE;
        core.fragmentStoresAndAtomics = features.d_int64FragmentAtomics
                                            ? VK_TRUE
                                            : VK_FALSE;
        chain.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters =
            choice->d_drawParameters ? VK_TRUE : VK_FALSE;
        chain.get<vk::PhysicalDeviceVulkan12Features>()
            .shaderBufferInt64Atomics = features.d_int64BufferAtomics
                                            ? VK_TRUE
                                            : VK_FALSE;
        auto& v13            = chain.get<vk::PhysicalDeviceVulkan13Features>();
        v13.dynamicRendering = VK_TRUE;
        v13.synchronization2 = VK_TRUE;
        if (features.d_meshShaders) {
            chain.get<vk::PhysicalDeviceMeshShaderFeaturesEXT>().meshShader =
                VK_TRUE;
        } else {
            chain.unlink<vk::PhysicalDeviceMeshShaderFeaturesEXT>();
        }

        auto& deviceCreateInfo = chain.get<vk::DeviceCreateInfo>();
        deviceCreateInfo.setQueueCreateInfos(queueCreateInfo);
        deviceCreateInfo.setPEnabledExtensionNames(deviceExtensions);

        device        = vk::raii::Device(choice->d_device, deviceCreateInfo);
        graphicsQueue = vk::raii::Queue(device, choice->d_queueFamily, 0);

        vk::CommandPoolCreateInfo commandPoolCreateInfo{};
        commandPoolCreateInfo.flags =
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        commandPoolCreateInfo.queueFamilyIndex = choice->d_queueFamily;
        commandPool = vk::raii::CommandPool(device, commandPoolCreateInfo);

        std::array<vk::DescriptorSetLayoutBinding, k_MAX_BINDINGS> bindings;
        for (std::uint32_t i = 0; i < k_MAX_BINDINGS; ++i) {
            bindings[i] = {i,
                           vk::DescriptorType::eStorageBuffer,
                           1,
                           vk::ShaderStageFlagBits::eAll};
        }

        const vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings);
        descriptorSetLayout = vk::raii::DescriptorSetLayout(device,
                                                            layoutInfo);

        const vk::DescriptorSetLayout setLayout = *descriptorSetLayout;
        vk::PipelineLayoutCreateInfo  pipelineLayoutInfo{};
        pipelineLayoutInfo.setSetLayouts(setLayout);

        pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);


        constexpr std::uint32_t maxSets = 1024;

        std::array poolSizes = {
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer,
                                   maxSets * k_MAX_BINDINGS),
        };

        vk::DescriptorPoolCreateInfo descriptorPoolInfo{};
        descriptorPoolInfo.maxSets = maxSets;
        descriptorPoolInfo.flags =
            vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        descriptorPoolInfo.setPoolSizes(poolSizes);

        descriptorPool = vk::raii::DescriptorPool(device, descriptorPoolInfo);

        vk::QueryPoolCreateInfo queryPoolInfo{};
        queryPoolInfo.queryType  = vk::QueryType::eTimestamp;
        queryPoolInfo.queryCount = 2;

        queryPool = vk::raii::QueryPool(device, queryPoolInfo);

    } catch (const std::exception& e) {
        return fail("cannot create the Vulkan device or its layouts: {}",
                    e.what());
    }

    return std::make_unique<VulkanGpu>(
        std::move(context),
        std::move(instance),
        choice->d_device,
        choice->d_queueFamily,
        std::move(device),
        std::move(commandPool),
        std::move(descriptorSetLayout),
        std::move(pipelineLayout),
        std::move(graphicsQueue),
        std::move(descriptorPool),
        std::move(queryPool),
        features,
        choice->d_device.getProperties().limits.timestampPeriod,
        choice->d_timestampValidBits);
}

}  // close namespace bench
}  // close namespace gamegine
