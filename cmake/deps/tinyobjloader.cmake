# tinyobjloader.cmake - Setup tinyobjloader (header-only)
include_guard()

# Try to find tinyobjloader in common locations
find_path(TINYOBJLOADER_INCLUDE_DIR
    NAMES tiny_obj_loader.h
    PATHS
        ${CMAKE_SOURCE_DIR}/external/lib
        ${CMAKE_SOURCE_DIR}/third_party/tinyobjloader
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinyobjloader
        ${CMAKE_SOURCE_DIR}/external/tinyobjloader
        /usr/local/include
        /opt/homebrew/include
        ENV TINYOBJLOADER_INCLUDE_DIR
)

if(TINYOBJLOADER_INCLUDE_DIR)
    # Found existing installation
    if(NOT TARGET tinyobjloader::tinyobjloader)
        add_library(tinyobjloader INTERFACE)
        add_library(tinyobjloader::tinyobjloader ALIAS tinyobjloader)
        target_include_directories(tinyobjloader INTERFACE
            ${TINYOBJLOADER_INCLUDE_DIR}
        )
    endif()
    message(STATUS "Found tinyobjloader at: ${TINYOBJLOADER_INCLUDE_DIR}")
else()
    message(FATAL_ERROR 
        "tinyobjloader not found! Please download tiny_obj_loader.h and place it in:\n"
        "  ${CMAKE_SOURCE_DIR}/external/lib/\n"
        "  or set TINYOBJLOADER_INCLUDE_DIR environment variable to the directory containing tiny_obj_loader.h\n"
        "Download from: https://raw.githubusercontent.com/tinyobjloader/tinyobjloader/main/tiny_obj_loader.h"
    )
endif()
