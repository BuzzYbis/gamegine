include_guard(GLOBAL)
include(FetchContent)

FetchContent_Declare(
        vulkan_headers
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
        GIT_TAG        vulkan-sdk-1.4.335.0 # pick a version
)

FetchContent_MakeAvailable(vulkan_headers)
