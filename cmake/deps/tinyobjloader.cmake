# tinyobjloader.cmake - Setup tinyobjloader (header-only)
include_guard()

set(TINYOBJ_DIR "${CMAKE_SOURCE_DIR}/third_party/tiny_obj")

if (NOT TARGET tinyobjloader::tinyobjloader)
    add_library(tinyobjloader INTERFACE)
    add_library(tinyobjloader::tinyobjloader ALIAS tinyobjloader)

    target_include_directories(tinyobjloader INTERFACE
            "${TINYOBJ_DIR}"
    )
endif ()

message(STATUS "Loaded bundled tinyobjloader from: ${TINYOBJ_DIR}")