// vlk_context.cpp                                                    -*-C++-*-
#include <rhi/vlk/vlk_context.h>

// std
#include <iostream>
#include <set>

// rhi
#include "core/core_log.h"

#include <rhi/rhi_types.h>
#include <rhi/vlk/vlk_buffer.h>
#include <rhi/vlk/vlk_commandlist.h>
#include <rhi/vlk/vlk_pipeline.h>
#include <rhi/vlk/vlk_resourcelayout.h>
#include <rhi/vlk/vlk_resourceset.h>
#include <rhi/vlk/vlk_swapchain.h>
#include <rhi/vlk/vlk_texture.h>
#include <rhi/vlk/vlk_utils.h>

namespace eng::rhi::vlk {
namespace {

// Device Extensions for Swapchain
const std::vector deviceExtensions = {
    vk::KHRSwapchainExtensionName,
    vk::KHRSpirv14ExtensionName,
#if defined(__APPLE__)
    // The "VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME" is required on macOS
    // This feature is still on VK_ENABLE_BETA_EXTENSIONS, which is why we
    // need to define it with its literal string
    "VK_KHR_portability_subset",
#endif
};

bool checkExtensionsSupport(
    const std::vector<const char*>&             requiredExtensions,
    const std::vector<vk::ExtensionProperties>& availableExtensions)
{
    for (const auto& required : requiredExtensions) {
        const bool found = std::ranges::any_of(
            availableExtensions,
            [required](const auto& prop) {
                return strcmp(prop.extensionName, required) == 0;
            });

        if (!found) {
            std::cerr << "[Error] Missing required extension: " << required
                      << std::endl;
            return false;
        }
    }
    return true;
}

bool checkLayersSupport(
    const std::vector<const char*>&         requiredLayers,
    const std::vector<vk::LayerProperties>& availableLayers)
{
    for (const auto& required : requiredLayers) {
        const bool found =
            std::ranges::any_of(availableLayers, [required](const auto& prop) {
                return strcmp(prop.layerName, required) == 0;
            });

        if (!found) {
            std::cerr << "[Error] Missing required layer: " << required
                      << std::endl;
            return false;
        }
    }
    return true;
}

bool checkDeviceExtensions(const vk::PhysicalDevice        device,
                           const std::vector<const char*>& requiredExtensions)
{
    const auto availableExtensions =
        device.enumerateDeviceExtensionProperties();

    // For all required extensions, we check whether they are among the
    // available extensions.
    for (const char* required : requiredExtensions) {
        const bool found = std::ranges::any_of(
            availableExtensions,
            [required](const vk::ExtensionProperties& pop) {
                return strcmp(required, pop.extensionName) == 0;
            });

        if (!found) {
            std::cerr << "[Error] Missing required extension: " << required
                      << std::endl;
            return false;
        }
    }
    return true;
}

bool checkDeviceFeatures(const vk::PhysicalDevice device)
{
    // We chain structures to request features from different Vulkan versions
    auto features =
        device
            .getFeatures2<vk::PhysicalDeviceFeatures2,
                          vk::PhysicalDeviceVulkan13Features,
                          vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

    const auto& basic = features.get<vk::PhysicalDeviceFeatures2>().features;
    const auto& v13   = features.get<vk::PhysicalDeviceVulkan13Features>();
    const auto& ext =
        features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

    return basic.samplerAnisotropy &&  // Sharper textures at angles
           v13.dynamicRendering &&     // Render without RenderPass objects
           ext.extendedDynamicState;   // More dynamic pipeline states
}

bool checkDeviceQueues(const vk::PhysicalDevice device,
                       const vk::SurfaceKHR     surface)
{
    const auto queueFamilies = device.getQueueFamilyProperties();

    for (size_t i = 0; i < queueFamilies.size(); ++i) {
        // Check for Drawing capability
        const bool supportsGraphics = !!(queueFamilies[i].queueFlags &
                                         vk::QueueFlagBits::eGraphics);

        // Check for Screen Presentation capability
        const bool supportsPresent =
            device.getSurfaceSupportKHR(static_cast<uint32_t>(i), surface);

        if (supportsGraphics && supportsPresent) {
            return true;
        }
    }
    return false;
}

}  // close unnamed namespace

Context::Context(core::Window* platform)
: d_instance(nullptr)
, d_physicalDevice(nullptr)
, d_device(nullptr)
, d_graphicsQueue(nullptr)
, d_surface(nullptr)
, d_window_p(platform)
, d_msaaSamples(vk::SampleCountFlagBits::e1)
, d_graphicsQueueFamilyIndex(0)
, d_commandPool(nullptr)
, d_descriptorPool(nullptr)
{
}

bool Context::initialize(const bool enableValidation)
{
    // Core Vulkan Setup
    if (!createInstance(enableValidation)) {
        return false;
    }

    if (!createSurface()) {
        return false;
    }

    // Hardware Selection
    if (!pickPhysicalDevice()) {
        return false;
    }

    if (!findGraphicsQueueFamily()) {
        return false;
    }

    d_msaaSamples = findMaxUsableSampleCount();

    // Logical Device & Queues
    if (!createLogicalDevice()) {
        return false;
    }

    // Resource Pools
    std::array poolSizes = {
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1000),
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler,
                               1000)};

    vk::DescriptorPoolCreateInfo descriptorpoolInfo{};
    descriptorpoolInfo.maxSets = 1000;
    descriptorpoolInfo.flags =
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    descriptorpoolInfo.setPoolSizes(poolSizes);

    d_descriptorPool = vk::raii::DescriptorPool(d_device, descriptorpoolInfo);

    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex();
    d_commandPool             = vk::raii::CommandPool(d_device, poolInfo);

    return true;
}

std::string Context::deviceName() const
{
    if (!*d_physicalDevice) {
        return "unknown";
    }

    return d_physicalDevice.getProperties().deviceName;
}

void Context::waitIdle()
{
    d_device.waitIdle();
}

bool Context::createInstance(const bool enableValidation)
{
    try {
        // Application Info
        // Note: Setting apiVersion is critical to unlock Vulkan 1.3 features
        // (like Dynamic Rendering) in the physical device later.
        vk::ApplicationInfo appInfo{};
        appInfo.pApplicationName   = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName        = "Gamegine";
        appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion         = vk::ApiVersion13;

        // Validation Layers
        std::vector<const char*> layers;
        if (enableValidation) {
            layers.push_back("VK_LAYER_KHRONOS_validation");
        }

        if (!checkLayersSupport(
                layers,
                d_context.enumerateInstanceLayerProperties())) {
            std::cerr << "Failed to find all required layers." << std::endl;
            return false;
        }

        // Instance Extensions (Windowing + Platform specifics)
        std::vector<const char*> extensions;
        uint32_t                 glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(
            &glfwExtensionCount);
        extensions.insert(extensions.end(),
                          glfwExtensions,
                          glfwExtensions + glfwExtensionCount);

#if defined(__APPLE__)
        // Required for MoltenVK portability enumeration on macOS
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

        if (enableValidation) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        if (!checkExtensionsSupport(
                extensions,
                d_context.enumerateInstanceExtensionProperties())) {
            std::cerr << "Failed to find all required extensions."
                      << std::endl;
            return false;
        }

        // Instance Creation
        vk::InstanceCreateInfo createInfo{};
        createInfo.pApplicationInfo    = &appInfo;
        createInfo.enabledLayerCount   = static_cast<uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames = layers.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(
            extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

#if defined(__APPLE__)
        createInfo.flags =
            vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif

        d_instance = vk::raii::Instance(d_context, createInfo);
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to create Vulkan Instance: " << e.what()
                  << std::endl;
        return false;
    }

    return true;
}

bool Context::createSurface()
{
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(*d_instance,
                                d_window_p->window(),
                                nullptr,
                                &surface) != VK_SUCCESS) {
        std::cerr << "Failed to create window surface." << std::endl;
        return false;
    }
    d_surface = vk::raii::SurfaceKHR(d_instance, surface);
    return true;
}

bool Context::pickPhysicalDevice()
{
    std::vector<vk::raii::PhysicalDevice> devices =
        d_instance.enumeratePhysicalDevices();
    const auto devIter =
        std::ranges::find_if(devices, [&](auto const& device) {
            // Check if the device supports the Vulkan 1.3 API version.
            if (device.getProperties().apiVersion < VK_API_VERSION_1_3) {
                return false;
            }

            // Check if all required device extensions are available.
            if (!checkDeviceExtensions(device, deviceExtensions)) {
                return false;
            }

            // Check if any of the queue families support graphics operations.
            if (!checkDeviceQueues(device, *d_surface)) {
                return false;
            }

            // Check if the device supports rendering features we plan to use.
            if (!checkDeviceFeatures(device)) {
                return false;
            }

            return true;
        });

    if (devIter != devices.end()) {
        d_physicalDevice = *devIter;
    }
    else {
        std::cerr << "Failed to find a supported physical device" << std::endl;
        return false;
    }
    return true;
}

vk::SampleCountFlagBits Context::findMaxUsableSampleCount() const
{
    const vk::PhysicalDeviceProperties physicalDeviceProperties =
        d_physicalDevice.getProperties();

    const vk::SampleCountFlags counts =
        physicalDeviceProperties.limits.framebufferColorSampleCounts &
        physicalDeviceProperties.limits.framebufferDepthSampleCounts;

    if (counts & vk::SampleCountFlagBits::e64) {
        return vk::SampleCountFlagBits::e64;
    }

    if (counts & vk::SampleCountFlagBits::e32) {
        return vk::SampleCountFlagBits::e32;
    }

    if (counts & vk::SampleCountFlagBits::e16) {
        return vk::SampleCountFlagBits::e16;
    }

    if (counts & vk::SampleCountFlagBits::e8) {
        return vk::SampleCountFlagBits::e8;
    }

    if (counts & vk::SampleCountFlagBits::e4) {
        return vk::SampleCountFlagBits::e4;
    }

    if (counts & vk::SampleCountFlagBits::e2) {
        return vk::SampleCountFlagBits::e2;
    }

    return vk::SampleCountFlagBits::e1;
}

bool Context::findGraphicsQueueFamily()
{
    // Enumerate all the properties of the queue family for the selected GPU.
    const auto queueFamilies = d_physicalDevice.getQueueFamilyProperties();
    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        // Check whether the family supports graphics operations.
        const vk::Flags<vk::QueueFlagBits> supportsGraphics =
            queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics;

        // Check whether the family supports the display on our surface.
        const bool supportsPresent =
            d_physicalDevice.getSurfaceSupportKHR(i, *d_surface);

        if (supportsGraphics && supportsPresent) {
            d_graphicsQueueFamilyIndex = i;
            return true;
        }
    }

    std::cerr << "No queue family found that supports both Graphics and "
                 "Presentation!"
              << std::endl;
    return false;
}

bool Context::createLogicalDevice()
{
    try {
        float                     queuePriority = 1.0f;
        vk::DeviceQueueCreateInfo queueCreateInfo({},
                                                  d_graphicsQueueFamilyIndex,
                                                  1,
                                                  &queuePriority);

        // Query physical device features first.
        auto supportedFeatures = d_physicalDevice.getFeatures2<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan11Features,
            vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

        const auto& supportedVk13Features =
            supportedFeatures.get<vk::PhysicalDeviceVulkan13Features>();

        // Setup Features - explicitly enable features if supported.
        vk::PhysicalDeviceVulkan13Features vk13Features{};

        if (supportedVk13Features.dynamicRendering) {
            vk13Features.dynamicRendering = VK_TRUE;
        }

        if (supportedVk13Features.synchronization2) {
            vk13Features.synchronization2 = VK_TRUE;
        }

        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
                    extendedDynamicState{};
        const auto& supportedExtendedDynamicState =
            supportedFeatures
                .get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        if (supportedExtendedDynamicState.extendedDynamicState) {
            extendedDynamicState.extendedDynamicState = VK_TRUE;
        }

        // Setup base features (samplerAnisotropy)
        vk::PhysicalDeviceFeatures2 baseFeatures{};
        const auto&                 supportedFeatures2 =
            supportedFeatures.get<vk::PhysicalDeviceFeatures2>();

        if (supportedFeatures2.features.samplerAnisotropy) {
            baseFeatures.features.samplerAnisotropy = VK_TRUE;
        }

        if (supportedFeatures2.features.largePoints) {
            baseFeatures.features.largePoints = VK_TRUE;
        }

        // Enable non-solid fill mode to support WireFrame rendering.
        if (supportedFeatures2.features.fillModeNonSolid) {
            baseFeatures.features.fillModeNonSolid = VK_TRUE;
        }

        // Manually set up the pNext chain to ensure it's correct.
        extendedDynamicState.pNext = nullptr;
        vk13Features.pNext         = &extendedDynamicState;
        vk::PhysicalDeviceVulkan11Features vk11Features{};
        vk11Features.pNext = &vk13Features;
        baseFeatures.pNext = &vk11Features;

        // Setup Device Info
        vk::DeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.pNext                 = &baseFeatures;
        deviceCreateInfo.queueCreateInfoCount  = 1;
        deviceCreateInfo.pQueueCreateInfos     = &queueCreateInfo;
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(
            deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

        // Create the Logical Device.
        d_device = vk::raii::Device(d_physicalDevice, deviceCreateInfo);

        // Retrieve the handle to the Graphics Queue.
        d_graphicsQueue = vk::raii::Queue(d_device,
                                          d_graphicsQueueFamilyIndex,
                                          0);
    }
    catch (const std::exception& e) {
        std::cerr << "[Critical] Failed to create Logical Device: " << e.what()
                  << std::endl;
        return false;
    }
    return true;
}

std::unique_ptr<SwapchainProtocol> Context::createSwapchain(uint32_t width,
                                                            uint32_t height)
{
    auto swapchain = std::make_unique<Swapchain>(this, width, height);
    swapchain->initialize();
    return swapchain;
}

std::unique_ptr<CommandListProtocol> Context::createCommandList()
{
    return std::make_unique<CommandList>(this);
}

std::unique_ptr<BufferProtocol> Context::createBuffer(size_t      size,
                                                      BufferUsage usage)
{
    return std::make_unique<Buffer>(this, size, usage);
}

std::unique_ptr<PipelineProtocol>
Context::createPipeline(const PipelineConfig& config)
{
    return std::make_unique<Pipeline>(this, config);
}

std::unique_ptr<TextureProtocol>
Context::createTexture(const uint32_t width,
                       const uint32_t height,
                       const uint32_t mipLevels,
                       const Format   format,
                       const void*    pixels)
{
    return std::make_unique<Texture>(this,
                                     width,
                                     height,
                                     mipLevels,
                                     Utils::getVkFormat(format),
                                     pixels);
}

std::unique_ptr<ResourceLayoutProtocol>
Context::createResourceLayout(const ResourceLayoutConfig& config)
{
    return std::make_unique<ResourceLayout>(this, config);
}

std::unique_ptr<ResourceSetProtocol>
Context::createResourceSet(ResourceLayoutProtocol* layout)
{
    auto* vlkLayout = static_cast<ResourceLayout*>(layout);
    return std::make_unique<ResourceSet>(this, vlkLayout);
}

}  // close package namespace
