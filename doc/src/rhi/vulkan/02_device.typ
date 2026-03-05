

== Selecting the Physical Device

Once the instance is initialized, we have a connection to the Vulkan driver. However, we are not yet talking to a specific Graphics Card (GPU). A computer might have multiple GPUs (e.g., an integrated Intel chip and a dedicated NVIDIA card).

We need to iterate through all available GPU, inspect their capabilities, and pick the best one our engine.


=== Enumeration

==== Concept

The first step is simply to ask the driver what GPU are available.


==== Implementation

```cpp
// Get a list of all GPUs (Physical Devices)
std::vector<vk::raii::PhysicalDevice> devices = d_instance.enumeratePhysicalDevices();
```


=== The Selection Checklist

We cannot simply pick the first device in the list. We need to filter them based on a strict checklist. If a device fails any of these checks, it is discarded.


==== Vulkan API Version

===== Concept

Hardware capabilities evolve. Our engine relis on modern achitecture feature, specifically *Dynamic Rendering*, which allows us to draw geometry without creation complex Render Pass object. This feature became core in *Vulkan 1.3*.


===== Implementation

We verify that the device driver supports at least Vulkan 1.3.

```cpp
const auto devIter =
    std::ranges::find_if(devices, [&](auto const& device) {
        // Check if the device supports the Vulkan 1.3 API version or newer
        if (device.getProperties().apiVersion < VK_API_VERSION_1_3) {
            return false;
        }
        // ...
    });
```


==== Device Extensions

===== Concept

While the _instance_ handles global concepts (like the OS window), the _Physical Device_ need specific dreivers to draw into that window.

- Generic: We need `VK_KHR_swapchain` to allow the GPU to present images to the screen.
- MacOS: Apple's GPUs do not natively support Vulkan. We use MooltenVK (a translator), which requires the `VK_KHR_portability_subset` extensio to handle non-compliant behaviours.


===== Implementation

We check if the device supports all required extensions.

```cpp
const std::vector deviceExtensions = {
    vk::KHRSwapchainExtensionName, // Essential for rendering to a window
    vk::KHRSpirv14ExtensionName,
#if defined(__APPLE__)
    "VK_KHR_portability_subset",   // Essential for MoltenVK
#endif
};

bool
checkDeviceExtensions(const vk::PhysicalDevice        device,
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
```


==== Queue Families

===== Concept

This is a unique concept in Vulkan. Think of the GPU as a factory. inside the factory, there are different types of Conveyor Belts (Queue):

1. Graphics Queue: Can process drawing commands (triangles).
2. Compute Queue: Can process math commands (physics, AI).
3. Transfer Queue: Optimized for moving data (uploading textures).
4. Present Queue: Can hand off the finished image to the monitor.

These queues are grouped into Families. A "Universal" family might support Graphics, Compute and Transfer all at once.

We must ensure the device has at least one family capable of *Graphics* and one capable of *Presenting*.


===== Implementation

We iterate over the families to find the indices we need.

```cpp
static bool checkDeviceQueues(const vk::PhysicalDevice device,
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
        // Cehck for Screen Presentation capability
        if (device.getSurfaceSupportKHR(static_cast<uint32_t>(i), surface)) {
            hasPresent = true;
        }
        if (hasGraphics && hasPresent) {
            return true;
        }
    }
    return false;
}
```


==== Physical Device Features

===== Concept

Just because a GPU is powerful doens't mean it supports every specific rendering technique. Features are boolean toggles representing specific capabilities.

For our engine, we need:
- Anisotropic Filtering: For crisp textures at oblique angles.
- Dynamic Rendering: To simplify our render loop (Vulkan 1.3).
- Extended Dynamic State: To change line widths or topology without recompiling pipelines.


===== Implementation

We use the `pNext` chain pattern (a linked list of structs) to query standard 1.0 features, 1.3 features and extensions simultaneously.

```cpp
static bool checkDeviceFeatures(const vk::PhysicalDevice device)
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
```


== Communication Interface & Logical Device

After selecting our Physical Device, we cannot simply start sending it commands. We need to establish a communication protocol. This involves tree steps:
1. Creating a *Surface* to talk to the OS window.
2. Finding the right *Queue Family* to execute our commands.
3. Creating a *Logical Device* to interface with the hardware and enable specific feature

=== The Surface

==== Concept

Vulkan is a platform-agnostic API; it technically doesn't know what a "window" is on Windows, macOS or Linux. However, to draw pixels on the screen, it needs a connection to the native window system.

This connection is called the *Surface*. It is an abstract handle that represents the raw window to the Vulkan driver.


==== Implementation

In our engine, this complexity is abstracted by the GLFW library.

```cpp
bool VulkanContext::createSurface()
{
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(*d_instance,
                                d_platform_p->window(),
                                nullptr,
                                &surface) != VK_SUCCESS) {
        std::cerr << "Failed to create window surface." << std::endl;
        return false;
    }
    d_surface = vk::raii::SurfaceKHR(d_instance, surface);
    return true;
}
```


=== Queue Families

==== Concept

As explained earlier, the GPU does not execute commands instantly. We must place them into Queues.

We already know that we need a specific type of queue that can to *Graphics* and *Presentation*.


==== Implementation

We iterate throughall available families and look for one that satisfies both requirements.

```cpp
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
```


=== The Logical Device

==== Concept

This is the final step of initialization

Creating the logical device is where we lock in our configuration. We tell the driver exactly which features we are going to use (like Dynamic Rendering). If we try to use a feature later that wasn't enabled here, the driver will crash.


==== Vulkan object

Vulkan uses a system called the `pNext` chain to extend its structures without breaking backward compatibility. instead of passing one giant struct with 500 booleans, we link smaller structs together like a linked list.


==== Implementation

In our engine, we chain:

```cpp
// Configure Vulkan 1.3 Features (Dynamic Rendering)
vk::PhysicalDeviceVulkan13Features vk13Features{};

if (supportedVk13Features.dynamicRendering) {
    vk13Features.dynamicRendering = VK_TRUE;
}

if (supportedVk13Features.synchronization2) {
    vk13Features.synchronization2 = VK_TRUE;
}

// Configure Extensions (Extended Dynamic State)
vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
            extendedDynamicState{};
const auto& supportedExtendedDynamicState =
    supportedFeatures
        .get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
if (supportedExtendedDynamicState.extendedDynamicState) {
    extendedDynamicState.extendedDynamicState = VK_TRUE;
}

// Configure Base Features (Anisotropy)
vk::PhysicalDeviceFeatures2 baseFeatures{};
if (supportedFeatures.get<vk::PhysicalDeviceFeatures2>()
        .features.samplerAnisotropy) {
    baseFeatures.features.samplerAnisotropy = VK_TRUE;
}

// Manually set up the pNext chain to ensure it's correct
// Base -> Vulkan 1.1 -> Vulkan 1.3 -> Extended State
extendedDynamicState.pNext = nullptr;
vk13Features.pNext         = &extendedDynamicState;
vk::PhysicalDeviceVulkan11Features vk11Features{};
vk11Features.pNext = &vk13Features;
baseFeatures.pNext = &vk11Features;
```


=== Device creation

Finally, we create the device using `vk::DeviceCreateInfo`. We pass it:
- The `pNetx` chain of features.
- The queue configuration (we want 1 graphics queue).
- The list of device extensions (Swapchain, etc.).


```cpp
// Setup Device Info
vk::DeviceCreateInfo deviceCreateInfo{};
deviceCreateInfo.pNext                 = &baseFeatures;
deviceCreateInfo.queueCreateInfoCount  = 1;
deviceCreateInfo.pQueueCreateInfos     = &queueCreateInfo;
deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(
    deviceExtensions.size());
deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

// Create the Logical Device
d_device        = vk::raii::Device(d_physicalDevice, deviceCreateInfo);

// Retrieve the handle to the Graphics Queue
d_graphicsQueue = vk::raii::Queue(d_device,
                                  d_graphicsQueueFamilyIndex,
                                  0);
```
