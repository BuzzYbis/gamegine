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
        core/input.cpp
        core/input.h
        ui/ui_panel.h
        ui/ui_manager.cpp
        ui/ui_manager.h
        core/log.cpp
        core/log.h
        asset/importers/obj_importer.cpp
        asset/importers/obj_importer.h
        ui/panels/entities_panel.cpp
        ui/panels/entities_panel.h
        ui/panels/game_panel.cpp
        ui/panels/game_panel.h
        asset/model_data.h
        asset/importers/model_importer.h
        rhi/vulkan/vk_render_target.cpp
        rhi/vulkan/vk_render_target.h
)

add_library(engine STATIC
        rhi/vulkan/vk_pipeline.cpp
        rhi/vulkan/vk_pipeline.h
        rhi/vulkan/vk_descriptor_manager.cpp
        rhi/vulkan/vk_descriptor_manager.h
        renderer/mesh.cpp
        renderer/mesh.h
        core/vertex.h
        rhi/vulkan/vk_buffer.cpp
        rhi/vulkan/vk_buffer.h
        renderer/texture.cpp
        renderer/texture.h
        core/window.h
        core/window.cpp
        core/engine.cpp
        core/engine.h
        renderer/renderer.cpp
        renderer/renderer.h
        renderer/command_pool.cpp
        renderer/command_pool.h
        rhi/vulkan/vk_swapchain.cpp
        rhi/vulkan/vk_swapchain.h
        rhi/vulkan/vk_context.cpp
        rhi/vulkan/vk_context.h
        rhi/vulkan/vk_resource_utils.cpp
        rhi/vulkan/vk_resource_utils.h
        core/step_timer.cpp
        core/step_timer.h
        renderer/material.cpp
        renderer/material.h
        scene/components.h
        scene/scene.h
        scene/entity.h
        asset/asset_manager.cpp
        asset/asset_manager.h
        scene/components/transform_component.h
        scene/components/camera_component.h
        scene/components/mesh_component.h
        scene/scene.cpp
        scene/system_manager.cpp
        scene/system_manager.h
        scene/systems/isystem.h
        scene/systems/camera_system.cpp
        scene/systems/camera_system.h
        scene/components/tag_component.h
        rhi/vulkan/vk_types.h
        ui/ui_context.cpp
        ui/ui_context.h
        ${IMGUI_SOURCES}
        ${TINY_GLTF_DIR}
        ../third_party/tiny_gltf/tiny_gltf_impl.cpp
)

target_include_directories(engine
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${TINY_GLTF_DIR}
)

# Link against Vulkan - if SDK found, use it; otherwise headers only
if (TARGET Vulkan::Headers)
    target_link_libraries(engine PUBLIC Vulkan::Headers)
    # If Vulkan SDK is found, also link the loader library explicitly
    if (Vulkan_FOUND AND Vulkan_LIBRARY)
        target_link_libraries(engine PUBLIC ${Vulkan_LIBRARY})
        # On macOS, set rpath to find the loader at runtime
        if (APPLE)
            get_filename_component(VULKAN_LIB_DIR ${Vulkan_LIBRARY} DIRECTORY)
            set_target_properties(engine PROPERTIES
                    INSTALL_RPATH "${VULKAN_LIB_DIR}"
                    BUILD_WITH_INSTALL_RPATH TRUE
            )
        endif ()
    endif ()
else ()
    message(FATAL_ERROR "Vulkan::Headers target not found. Vulkan SDK installation required.")
    message(FATAL_ERROR "On macOS: Install via 'brew install --cask vulkan-sdk' or download from https://vulkan.lunarg.com/")
endif ()

target_link_libraries(engine PUBLIC EnTT::EnTT glfw glm::glm stb::stb tinyobjloader::tinyobjloader)
