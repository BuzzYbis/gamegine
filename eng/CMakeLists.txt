cmake_minimum_required(VERSION 4.1)
project(core)

set(CMAKE_CXX_STANDARD 23)

# Vulkan is included in main CMakeLists.txt, but include it here too for safety
# (include_guard will prevent double processing)
include(${CMAKE_SOURCE_DIR}/cmake/deps/vulkan_headers.cmake)
include(${GAMEGINE_ROOT}/cmake/deps/glfw.cmake)
include(${GAMEGINE_ROOT}/cmake/deps/glm.cmake)
include(${GAMEGINE_ROOT}/cmake/deps/stb.cmake)
include(${GAMEGINE_ROOT}/cmake/deps/tinyobjloader.cmake)

# ImGui (core + GLFW and Vulkan backends)
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
set(TINY_GLTF_DIR ${CMAKE_SOURCE_DIR}/third_party/tiny_gltf)
set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
        core/core_input.cpp
        core/core_input.h
        core/core_log.cpp
        core/core_log.h
        ui/ui_manager.cpp
        ui/ui_manager.h
        asset/impt/impt_obj.cpp
        asset/impt/impt_obj.h
        ui/pnl/pnl_entities.cpp
        ui/pnl/pnl_entities.h
        ui/pnl/pnl_game.cpp
        ui/pnl/pnl_game.h
        asset/asset_modeldata.h

        asset/impt/impt_model.h
        rhi/rhi_factory.cpp
        rhi/rhi_factory.h
        rhi/rhi_contextprotocol.h
        rhi/vlk/vlk_context.cpp
        rhi/vlk/vlk_context.h
        rhi/vlk/vlk_swapchain.cpp
        rhi/vlk/vlk_swapchain.h
        rhi/rhi_swapchainprotocol.h
        rhi/rhi_commandlistprotocol.h
        rhi/vlk/vlk_commandlist.cpp
        rhi/vlk/vlk_commandlist.h
        rhi/rhi_types.h
        rhi/rhi_bufferprotocol.h
        rhi/vlk/vlk_buffer.cpp
        rhi/vlk/vlk_buffer.h
        rhi/rhi_pipelineprotocol.h
        rhi/vlk/vlk_pipeline.cpp
        rhi/vlk/vlk_pipeline.h
        rhi/rhi_resourcelayoutprotocol.h
        rhi/rhi_resourcesetprotocol.h
        rhi/vlk/vlk_resourcelayout.cpp
        rhi/vlk/vlk_resourcelayout.h
        rhi/vlk/vlk_resourceset.cpp
        rhi/vlk/vlk_resourceset.h
        rhi/vlk/vlk_utils.cpp
        rhi/vlk/vlk_utils.h
        rhi/vlk/vlk_texture.cpp
        rhi/vlk/vlk_texture.h
        rhi/vlk/vlk_rendertarget.cpp
        rhi/vlk/vlk_rendertarget.h
        ui/ui_backendprotocol.h
        ui/ui_vulkanbackend.cpp
        ui/ui_vulkanbackend.h
        ui/ui_factory.cpp
        ui/ui_factory.h
)

add_library(eng STATIC
        rnd/mesh.cpp
        rnd/mesh.h
        core/core_vertex.h
        rnd/texture.cpp
        rnd/texture.h
        core/core_window.h
        core/core_window.cpp
        core/core_engine.cpp
        core/core_engine.h
        rnd/renderer.cpp
        rnd/renderer.h
        core/core_steptimer.cpp
        core/core_steptimer.h
        rnd/material.cpp
        rnd/material.h
        scn/scn_component.h
        scn/scn_scene.h
        scn/scn_entity.h
        asset/asset_manager.cpp
        asset/asset_manager.h
        scn/comp/comp_transform.h
        scn/comp/comp_camera.h
        scn/comp/comp_mesh.h
        scn/scn_scene.cpp
        scn/scn_systemmanager.cpp
        scn/scn_systemmanager.h
        scn/sys/sys_isystem.h
        scn/sys/sys_camera.cpp
        scn/sys/sys_camera.h
        scn/comp/comp_tag.h
        ui/ui_context.cpp
        ui/ui_context.h
        ${IMGUI_SOURCES}
        ${TINY_GLTF_DIR}
        ../third_party/tiny_gltf/tiny_gltf_impl.cpp
)

target_include_directories(eng
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${TINY_GLTF_DIR}
)

# Link against Vulkan - if SDK found, use it; otherwise headers only
if (TARGET Vulkan::Headers)
    target_link_libraries(eng PUBLIC Vulkan::Headers)
    # If Vulkan SDK is found, also link the loader library explicitly
    if (Vulkan_FOUND AND Vulkan_LIBRARY)
        target_link_libraries(eng PUBLIC ${Vulkan_LIBRARY})
        # On macOS, set rpath to find the loader at runtime
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
    message(FATAL_ERROR "On macOS: Install via 'brew install --cask vulkan-sdk' or download from https://vulkan.lunarg.com/")
endif ()

target_link_libraries(eng PUBLIC EnTT::EnTT glfw glm::glm stb::stb tinyobjloader::tinyobjloader)
