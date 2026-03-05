include_guard(GLOBAL)

# First try to find Vulkan SDK (includes loader on macOS/Windows/Linux)
# On macOS, this requires Vulkan SDK to be installed (includes MoltenVK)
find_package(Vulkan QUIET)

# Check if find_package provided the target
set(VULKAN_HAS_TARGET FALSE)
if(TARGET Vulkan::Headers)
    set(VULKAN_HAS_TARGET TRUE)
    message(STATUS "Vulkan::Headers target provided by find_package")
endif()

# If find_package didn't work or didn't provide the target, try manual discovery
if(NOT Vulkan_FOUND OR NOT VULKAN_HAS_TARGET)
    # Check common installation paths for the loader
    find_library(VULKAN_LIBRARY_MANUAL
        NAMES vulkan
        PATHS
            /usr/local/lib
            /opt/homebrew/lib
            /usr/lib
            ${CMAKE_SYSROOT}/usr/lib
        NO_DEFAULT_PATH
    )
    
    if(VULKAN_LIBRARY_MANUAL)
        message(STATUS "Found Vulkan loader manually: ${VULKAN_LIBRARY_MANUAL}")
        # Find headers in common locations
        find_path(VULKAN_INCLUDE_DIR_MANUAL
            NAMES vulkan/vulkan.h
            PATHS
                /usr/local/include
                /opt/homebrew/include
                /usr/include
                ${CMAKE_SYSROOT}/usr/include
            NO_DEFAULT_PATH
        )
        
        if(VULKAN_INCLUDE_DIR_MANUAL)
            # Create an interface library to mimic Vulkan::Headers
            if(NOT TARGET Vulkan::Headers)
                add_library(Vulkan::Headers INTERFACE IMPORTED GLOBAL)
                set_target_properties(Vulkan::Headers PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${VULKAN_INCLUDE_DIR_MANUAL}"
                )
                # Store the library path and include dirs for linking
                set(Vulkan_LIBRARY "${VULKAN_LIBRARY_MANUAL}" CACHE FILEPATH "Vulkan loader library" FORCE)
                set(Vulkan_INCLUDE_DIRS "${VULKAN_INCLUDE_DIR_MANUAL}" CACHE PATH "Vulkan include directories" FORCE)
                set(Vulkan_FOUND TRUE CACHE BOOL "Vulkan found" FORCE)
                set(VULKAN_HAS_TARGET TRUE)
                message(STATUS "Created Vulkan::Headers target with manual discovery")
                message(STATUS "  Headers: ${VULKAN_INCLUDE_DIR_MANUAL}")
                message(STATUS "  Library: ${VULKAN_LIBRARY_MANUAL}")
            endif()
        else()
            message(WARNING "Found Vulkan library but not headers. Headers should be in /usr/local/include/vulkan/")
        endif()
    endif()
endif()

# If find_package found Vulkan but didn't provide the target, try to create it from the found variables
if(Vulkan_FOUND AND NOT VULKAN_HAS_TARGET AND Vulkan_INCLUDE_DIRS)
    if(NOT TARGET Vulkan::Headers)
        add_library(Vulkan::Headers INTERFACE IMPORTED GLOBAL)
        set_target_properties(Vulkan::Headers PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${Vulkan_INCLUDE_DIRS}"
        )
        message(STATUS "Created Vulkan::Headers target from find_package variables")
    endif()
endif()

# Final check: ensure the target exists, create it if needed
if(Vulkan_FOUND AND NOT TARGET Vulkan::Headers)
    if(Vulkan_INCLUDE_DIRS)
        add_library(Vulkan::Headers INTERFACE IMPORTED GLOBAL)
        set_target_properties(Vulkan::Headers PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${Vulkan_INCLUDE_DIRS}"
        )
        message(STATUS "Created Vulkan::Headers target (final fallback)")
    endif()
endif()

# Check if we have Vulkan (either from find_package or manual discovery)
if(Vulkan_FOUND OR TARGET Vulkan::Headers)
    if(Vulkan_INCLUDE_DIRS)
        message(STATUS "Vulkan SDK found: ${Vulkan_INCLUDE_DIRS}")
    endif()
    # On macOS, ensure we can find the loader at runtime
    if(APPLE AND Vulkan_LIBRARY)
        message(STATUS "Vulkan loader found: ${Vulkan_LIBRARY}")
    endif()
    # Verify target exists
    if(TARGET Vulkan::Headers)
        message(STATUS "Vulkan::Headers target is available")
    else()
        message(WARNING "Vulkan found but Vulkan::Headers target is not available")
    endif()
else()
    # Fallback: use FetchContent for headers only (for development)
    # Note: This won't provide the loader, so runtime will fail
    message(WARNING "Vulkan SDK not found. Using headers only. Install Vulkan SDK for runtime support.")
    message(WARNING "On macOS: Install Vulkan SDK from https://vulkan.lunarg.com/ or via Homebrew: brew install --cask vulkan-sdk")
    message(WARNING "After installation, set VULKAN_SDK environment variable or ensure it's in PATH")
    include(FetchContent)
    
    FetchContent_Declare(
            vulkan_headers
            GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
            GIT_TAG        vulkan-sdk-1.4.335.0 # pick a version
    )
    
    FetchContent_MakeAvailable(vulkan_headers)
    # FetchContent provides Vulkan::Headers target
endif()
