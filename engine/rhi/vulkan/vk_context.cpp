// vk_context.cpp                                                     -*-C++-*-
#include <rhi/vulkan/vk_context.h>

// std
#include <iostream>
#include <set>

namespace engine::rhi::vulkan {
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
    std::set<std::string> required(requiredExtensions.begin(),
                                   requiredExtensions.end());

    for (const auto& extension : availableExtensions) {
        required.erase(extension.extensionName);
    }
    return required.empty();
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

    bool hasGraphics = false;
    bool hasPresent  = false;

    for (size_t i = 0; i < queueFamilies.size(); ++i) {
        // Check for Drawing capability
        if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            hasGraphics = true;
        }

        // Check for Screen Presentation capability
        if (device.getSurfaceSupportKHR(static_cast<uint32_t>(i), surface)) {
            hasPresent = true;
        }

        if (hasGraphics && hasPresent) {
            return true;
        }
    }
    return false;
}

}  // close unnamed namespace

VulkanContext::VulkanContext(core::Window* platform)
: d_window_p(platform)
{
}

bool VulkanContext::initialize(const bool enableValidation)
{
    // Create Vulkan instance
    if (!createInstance(enableValidation)) {
        return false;
    }

    // Create surfarce
    if (!createSurface()) {
        return false;
    }

    // Find a suitable physical device (GPU)
    if (!pickPhysicalDevice()) {
        return false;
    }

    // Use 1 sample so pipeline and swapchain attachments match (no MSAA).
    // Re-enable findMaxUsableSampleCount() when pipeline creation consistently
    // uses the same sample count as the dynamic rendering attachments.
    d_msaaSamples = vk::SampleCountFlagBits::e1;
    // d_msaaSamples = findMaxUsableSampleCount();

    // Select queue family
    if (!findGraphicsQueueFamily()) {
        return false;
    }

    // Create logical device
    if (!createLogicalDevice()) {
        return false;
    }

    return true;
}

bool VulkanContext::createInstance(const bool enableValidation)
{
    try {
        // Setup Application Info
        vk::ApplicationInfo appInfo{};
        appInfo.pApplicationName   = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName        = "Gamegine";
        appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion         = vk::ApiVersion13;

        // Since Vulkan is an API, we need extensions to interface with the
        // window system
        std::vector<const char*> extensions;
        uint32_t                 glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(
            &glfwExtensionCount);
        extensions.insert(extensions.end(),
                          glfwExtensions,
                          glfwExtensions + glfwExtensionCount);

#if defined(__APPLE__)
        // Required for MoltenVK portability enumeration
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

        if (enableValidation) {
            // Add debug utils extension for validation
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        // Check if the required extensions are supported by Vulkan
        if (!checkExtensionsSupport(
                extensions,
                d_context.enumerateInstanceExtensionProperties())) {
            std::cerr << "Failed to find all required extensions."
                      << std::endl;
            return false;
        }

        // Vulkan is designed to limited error checking, a validation layer can
        // be added to the instance to help catch errors
        std::vector<const char*> layers;
        if (enableValidation) {
            // Add the Khronos validation layer to handle errors and warnings
            layers.push_back("VK_LAYER_KHRONOS_validation");
        }

        if (!checkLayersSupport(
                layers,
                d_context.enumerateInstanceLayerProperties())) {
            std::cerr << "Failed to find all required layers." << std::endl;
            return false;
        }

        // Setting global extensions and validation layers
        vk::InstanceCreateInfo createInfo{};
        createInfo.pApplicationInfo    = &appInfo;
        createInfo.enabledLayerCount   = static_cast<uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames = layers.data();

#if defined(__APPLE__)
        // Required for MoltenVK portability enumeration
        createInfo.flags =
            vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif

        createInfo.enabledExtensionCount = static_cast<uint32_t>(
            extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // Initialize the Vulkan library by creating an instance
        // This is the connection between the application and the Vulkan API
        d_instance = vk::raii::Instance(d_context, createInfo);
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to create Vulkan Instance:" << e.what()
                  << std::endl;
        return false;
    }

    return true;
}

bool VulkanContext::createSurface()
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

bool VulkanContext::pickPhysicalDevice()
{
    std::vector<vk::raii::PhysicalDevice> devices =
        d_instance.enumeratePhysicalDevices();
    const auto devIter =
        std::ranges::find_if(devices, [&](auto const& device) {
            // Check if the device supports the Vulkan 1.3 API version
            if (device.getProperties().apiVersion < VK_API_VERSION_1_3) {
                return false;
            }

            // Check if all required device extensions are available
            if (!checkDeviceExtensions(device, deviceExtensions)) {
                return false;
            }

            // Check if any of the queue families support graphics operations
            if (!checkDeviceQueues(device, *d_surface)) {
                return false;
            }

            // Check if the device supports rendering features we plan to use
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

vk::SampleCountFlagBits VulkanContext::findMaxUsableSampleCount() const
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

bool VulkanContext::findGraphicsQueueFamily()
{
    // Enumerate all the properties of the queue family for the selected GPU
    const auto queueFamilies = d_physicalDevice.getQueueFamilyProperties();
    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        // Check whether the family supports graphics operations
        const vk::Flags<vk::QueueFlagBits> supportsGraphics =
            queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics;

        // Check whether the family supports the display on our surface
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

bool VulkanContext::createLogicalDevice()
{
    try {
        float                     queuePriority = 1.0f;
        vk::DeviceQueueCreateInfo queueCreateInfo({},
                                                  d_graphicsQueueFamilyIndex,
                                                  1,
                                                  &queuePriority);

        // Query physical device features first
        auto supportedFeatures = d_physicalDevice.getFeatures2<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan11Features,
            vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

        const auto& supportedVk13Features =
            supportedFeatures.get<vk::PhysicalDeviceVulkan13Features>();

        // Setup Features - explicitly enable features if supported
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

        // By default, Vulkan only draws filled triangles
        // (VK_POLYGON_MODE_FILL) but since we implement the "WireFrame"
        // pipeline to draw the edges of a model we need to enable
        // VK_POLYGON_MODE_LINE
        if (supportedFeatures2.features.fillModeNonSolid) {
            baseFeatures.features.fillModeNonSolid = VK_TRUE;
        }

        // Manually set up the pNext chain to ensure it's correct
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

        // Create the Logical Device
        d_device = vk::raii::Device(d_physicalDevice, deviceCreateInfo);

        // Retrieve the handle to the Graphics Queue
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

}  // close engine::rhi::vulkan namespace