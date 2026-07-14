# tinygltfloader.cmake - Setup tinygltfloader (header-only)
include_guard()

set(TINYGLTF_DIR "${CMAKE_SOURCE_DIR}/third_party/tiny_gltf")

if (NOT TARGET tinygltfloader::tinygltfloader)
    add_library(tinygltfloader INTERFACE)
    add_library(tinygltfloader::tinygltfloader ALIAS tinygltfloader)

    target_include_directories(tinygltfloader INTERFACE
            "${TINYGLTF_DIR}"
    )
endif ()

message(STATUS "Loaded bundled tinygltfloader from: ${TINYGLTF_DIR}")