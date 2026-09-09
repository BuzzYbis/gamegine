cmake_minimum_required(VERSION 4.1)
project(eng)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies ---
include(${CMAKE_SOURCE_DIR}/cmake/deps/vulkan_headers.cmake)
include(${GAMEGINE_ROOT}/cmake/deps/glfw.cmake)
include(${GAMEGINE_ROOT}/cmake/deps/glm.cmake)
include(${GAMEGINE_ROOT}/cmake/deps/stb.cmake)
include(${GAMEGINE_ROOT}/cmake/deps/tinyobjloader.cmake)
include(${GAMEGINE_ROOT}/cmake/deps/tinygltfloader.cmake)
include(${GAMEGINE_ROOT}/cmake/deps/ktx.cmake)

# --- Target Definition ---
add_library(eng STATIC)

# Profiling instrumentation. When disabled, the 'ENG_PROFILE_*' macros expand
# to nothing and the engine pays for no sample at all.
option(GAMEGINE_ENABLE_PROFILING "Enable the in-engine profiler" ON)
if (GAMEGINE_ENABLE_PROFILING)
    target_compile_definitions(eng PUBLIC ENG_ENABLE_PROFILING)
endif ()

# --- Build Information ---
# The identity of the build is captured at build time rather than at
# configure time, so that a benchmark row records the commit that actually
# produced the binary.
set(ENG_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(ENG_BUILDINFO_HEADER ${ENG_GENERATED_DIR}/core/core_buildinfo.h)

add_custom_target(eng_buildinfo
        COMMAND ${CMAKE_COMMAND}
        -DSRC=${GAMEGINE_ROOT}
        -DTEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/core/core_buildinfo.h.in
        -DOUTPUT=${ENG_BUILDINFO_HEADER}
        -DENG_VERSION=${CMAKE_PROJECT_VERSION}
        -DENG_BUILD_TYPE=$<CONFIG>
        -DENG_COMPILER=${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}
        -P ${GAMEGINE_ROOT}/cmake/git_info.cmake
        BYPRODUCTS ${ENG_BUILDINFO_HEADER}
        COMMENT "Capturing build information..."
)

add_dependencies(eng eng_buildinfo)

# --- Source Organization ---

# 1. Third Party: ImGui
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
target_sources(eng PRIVATE
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

# 2. Engine Core & Public API
target_sources(eng PRIVATE
        core/core_engine.cpp
        core/core_engine.h
        core/core_window.cpp
        core/core_window.h
        core/core_input.cpp
        core/core_input.h
        core/core_steptimer.cpp
        core/core_steptimer.h
        core/core_log.cpp
        core/core_log.h
        core/core_vertex.h
        core/core_instance.h
        core/core_profiler.cpp
        core/core_profiler.h
)

# 3. Renderer & RHI (Vulkan)
target_sources(eng PRIVATE
        rnd/renderer.cpp
        rnd/renderer.h
        rnd/mesh.cpp
        rnd/mesh.h
        rnd/meshbatch.h
        rnd/texture.cpp
        rnd/texture.h
        rnd/material.cpp
        rnd/material.h

        rhi/rhi_factory.cpp
        rhi/rhi_factory.h
        rhi/rhi_contextprotocol.h
        rhi/rhi_swapchainprotocol.h
        rhi/rhi_commandlistprotocol.h
        rhi/rhi_bufferprotocol.h
        rhi/rhi_pipelineprotocol.h
        rhi/rhi_resourcelayoutprotocol.h
        rhi/rhi_resourcesetprotocol.h
        rhi/rhi_types.h

        rhi/vlk/vlk_context.cpp
        rhi/vlk/vlk_context.h
        rhi/vlk/vlk_swapchain.cpp
        rhi/vlk/vlk_swapchain.h
        rhi/vlk/vlk_commandlist.cpp
        rhi/vlk/vlk_commandlist.h
        rhi/vlk/vlk_buffer.cpp
        rhi/vlk/vlk_buffer.h
        rhi/vlk/vlk_pipeline.cpp
        rhi/vlk/vlk_pipeline.h
        rhi/vlk/vlk_resourcelayout.cpp
        rhi/vlk/vlk_resourcelayout.h
        rhi/vlk/vlk_resourceset.cpp
        rhi/vlk/vlk_resourceset.h
        rhi/vlk/vlk_utils.cpp
        rhi/vlk/vlk_utils.h
        rhi/vlk/vlk_texture.cpp
        rhi/vlk/vlk_texture.h
)

# 4. Scene, UI & Procedural
target_sources(eng PRIVATE
        scn/scn_scene.cpp
        scn/scn_scene.h
        scn/scn_entity.h
        scn/scn_component.h
        scn/scn_systemmanager.h
        scn/scn_systemmanager.cpp
        scn/comp/comp_transform.h
        scn/comp/comp_camera.h
        scn/comp/comp_mesh.h
        scn/comp/comp_tag.h
        scn/sys/sys_isystem.h
        scn/sys/sys_camera.h
        scn/sys/sys_camera.cpp
        ui/ui_manager.cpp
        ui/ui_manager.h
        ui/ui_context.cpp
        ui/ui_context.h
        ui/ui_vulkanbackend.cpp
        ui/ui_vulkanbackend.h
        ui/ui_factory.cpp
        ui/ui_factory.h
        ui/ui_backendprotocol.h
        ui/ui_panel.h
        ui/pnl/pnl_profiler.cpp
        ui/pnl/pnl_profiler.h
)

# 5. Assets & Importers
set(TINY_GLTF_DIR ${CMAKE_SOURCE_DIR}/third_party/tiny_gltf)
target_sources(eng PRIVATE
        asset/asset_manager.cpp
        asset/asset_manager.h
        asset/asset_modeldata.h
        asset/asset_benchmark.cpp
        asset/asset_benchmark.h
        asset/impt/impt_gltf.cpp
        asset/impt/impt_gltf.h
        asset/impt/impt_model.h
)

# --- Includes ---
target_include_directories(eng
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENG_GENERATED_DIR}
)

# Third-party headers are marked SYSTEM so that their own warnings do not
# pollute the build of the engine.
target_include_directories(eng SYSTEM
        PUBLIC
        ${IMGUI_DIR}
        ${TINY_GLTF_DIR}
        ${FASTNOISE_INCLUDE_DIR}
        PRIVATE
        ${IMGUI_DIR}/backends
)

# --- Linking ---
if (TARGET Vulkan::Vulkan)
    target_link_libraries(eng PUBLIC Vulkan::Vulkan)
elseif (TARGET Vulkan::Headers)
    target_link_libraries(eng PUBLIC Vulkan::Headers)
    if (Vulkan_FOUND AND Vulkan_LIBRARY)
        target_link_libraries(eng PUBLIC ${Vulkan_LIBRARY})
        # macOS specific setup for raw library links
        if (APPLE)
            get_filename_component(VULKAN_LIB_DIR ${Vulkan_LIBRARY} DIRECTORY)
            set_target_properties(eng PROPERTIES
                    INSTALL_RPATH "${VULKAN_LIB_DIR}"
                    BUILD_WITH_INSTALL_RPATH TRUE
            )
        endif ()
    endif ()
else ()
    message(FATAL_ERROR "Vulkan::Headers target not found. Vulkan SDK installation required.")
endif ()

target_link_libraries(eng PUBLIC
        EnTT::EnTT
        glfw
        glm::glm
        stb::stb
        tinyobjloader::tinyobjloader
        KTX::ktx
)

# --- Warnings ---
if (MSVC)
    target_compile_options(eng PRIVATE /W4)
else ()
    target_compile_options(eng PRIVATE -Wall -Wextra)
endif ()
