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

# --- Target Definition ---
add_library(eng STATIC)

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
)

# 3. Renderer & RHI (Vulkan)
target_sources(eng PRIVATE
        rnd/renderer.cpp
        rnd/renderer.h
        rnd/mesh.cpp
        rnd/mesh.h
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
)

# 5. Assets & Importers
set(TINY_GLTF_DIR ${CMAKE_SOURCE_DIR}/third_party/tiny_gltf)
target_sources(eng PRIVATE
        asset/asset_manager.cpp
        asset/asset_manager.h
        asset/asset_modeldata.h
        asset/impt/impt_obj.cpp
        asset/impt/impt_obj.h
        asset/impt/impt_model.h
)

# --- Includes ---
target_include_directories(eng
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
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
)

# --- Warnings ---
if (MSVC)
    target_compile_options(eng PRIVATE /W4)
else ()
    target_compile_options(eng PRIVATE -Wall -Wextra)
endif ()
