#import "@preview/codly:1.3.0": *
#import "@preview/codly-languages:0.1.1": *
#show: codly-init.with()

== Vulkan instance

The instance is the very first step in initializing the Vulkan library. It establishes the foundational connection between our application and the underlying Vulkan library on the system. Unlike OpenGL, where context creation is often handled implicitly or by external libraries, Vulkan requires an explicit declaration of who we are and what global features we intend to use.


=== Application Info

==== Concept

Before we can ask the GPU driver to do any work, we must first introduce ourselves. When we initialize Vulkan, the driver (whether it's from NVIDIA, AMD, or Intel) wants to know two main things:
- Identity: "Who are you?" (Application Name, Engine Name).
- Capability: "Which version of Vulkan do we speak?" (API Version).

While we could technically don't give those information by passing nullptr, doing so in Vulkan is considered bad practice for two critical reasons:

- Driver Optimizations: GPU drivers are incredibly complex software containing database of specific game titles. If the driver recognizes our engine name (e.g., "Unreal Engine" or "Doom Eternal"), it can silently enable specific internal optimizations or workarounds for bugs known to affect that engine. Identifying our application allows us to benefit from these vendor-specific heuristics.
- Version Negotiation: This is the most practical reason. If we don't declare a specific version, Vulkan may default to version 1.0. This would prevent us from accessing the modern features introduced in Vulkan 1.2 or 1.3. (like Dynamic Rendering or Ray Tracing), even if the user's graphics card is brand new.


==== Vulkan object

To provide this information, Vulkan uses a structure called `ck::ApplicationInfo`. It is a simple container for strings and version numbers that we will pass to the instance creation method later.


==== Implementation

```cpp
vk::ApplicationInfo appInfo{};
appInfo.pApplicationName   = "Hello Triangle";
appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
appInfo.pEngineName        = "Gamegine";
appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
appInfo.apiVersion         = vk::ApiVersion14;
```


=== Validation layers

==== Concept

Vulkan is a low-level and explicit API. To minimize overhead, the driver performs almost no error checking at runtime. If we use an invalid handle or a wrong enum, the program will simply crash or produce undefined behavior (black screen) without explanation.

To solve this during development, we inject Validation Layers into the instance. These layers sit between the application and the driver to intercept API calls, check for errors, and report them.


==== Vulkan object

Layers are specified as a list of strings (names). The standard layer used today is `VK_LAYER_KHRONOS_validation`, which bundles all standard validation checks.


==== Implementation

We must first verify that the requested layers are actually installed on the machine to avoid a crash at startup.

```cpp
// 1. Verification Logic
bool checkLayersSupport(
    const std::vector<const char*>& requiredLayers,
    const std::vector<vk::LayerProperties>& availableLayers)
{
    for (const auto& required : requiredLayers) {
        const bool found = std::ranges::any_of(availableLayers, [required](const auto& prop) {
             return strcmp(prop.layerName, required) == 0;
        });
        if (!found) {
            std::cerr << "Missing required layer: " << required << std::endl;
            return false;
        }
    }
    return true;
}

// 2. Usage
std::vector<const char*> layers;
layers.push_back("VK_LAYER_KHRONOS_validation");

if (!checkLayersSupport(layers, d_context.enumerateInstanceLayerProperties())) {
    std::cerr << "Failed to find all required layers!" << std::endl;
}
```

Validation layers are typically enabled only in Debug builds and removed in Release builds to maximize performance.


=== Extensions

==== Concept

By default, Vulkan is a platform-agnostic API; it knows nothing about the operating system's window manager (Windows, Linux, macOS). To interface with the OS or use specific debug utilities, we must enable Instance Extensions.

Extensions can be platform-specific. For example, requesting ``VK_KHR_win32_surface on Linux will cause a crash.


==== Vulkan object

Like layers, extensions are passed as a list of strings.

- Surface Extensions: Usually retrieved via GLFW (`glfwGetRequiredInstanceExtensions`).
- Debug Utils: `VK_EXT_DEBUG_UTILS_EXTENSION_NAME` for receiving validation messages.
- Portability (macOS): `VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME` is required to run Vulkan on top of Metal (via MoltenVK).


==== Implementation

And like layers, we need to check that the exentions are supported

```cpp
std::vector<const char*> extensions;

// 1. Get GLFW extensions (Window Manager integration)
uint32_t glfwExtensionCount = 0;
const char** glfwExt = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
extensions.insert(extensions.end(), glfwExt, glfwExt + glfwExtensionCount);

// 2. Add Debug Utils
extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

// 3. MacOS specific (MoltenVK)
#if defined(__APPLE__)
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

// 4. Verification (Similar to layers logic)
if (!checkExtensionsSupport(extensions, d_context.enumerateInstanceExtensionProperties())) {
     std::cerr << "[Error] Missing required extension!" << std::endl;
}
```


=== Creating the Instance

==== Concept

Once we have define who we are (ApplicationInfo), what checks we want (Layers), and how we interact with the OS (Extensions), we package everything into a single descriptor to create the Instance.


==== Vulkan object

The `VkInstanceCreateInfo` struct aggregates all previous components.


==== Implementation

```cpp
vk::InstanceCreateInfo createInfo{};
createInfo.pApplicationInfo    = &appInfo;

// Layers
createInfo.enabledLayerCount   = static_cast<uint32_t>(layers.size());
createInfo.ppEnabledLayerNames = layers.data();

// Extensions
createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
createInfo.ppEnabledExtensionNames = extensions.data();

// MacOS Portability Flag
#if defined(__APPLE__)
    createInfo.flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif

// Final Creation (RAII wrapper)
d_instance = vk::raii::Instance(d_context, createInfo);
```
