include(${GAMEGINE_ROOT}/cmake/deps/glfw.cmake)
include(${GAMEGINE_ROOT}/cmake/deps/glm.cmake)
include(${GAMEGINE_ROOT}/cmake/deps/stb.cmake)
include(${GAMEGINE_ROOT}/cmake/deps/tinyobjloader.cmake)
include(${GAMEGINE_ROOT}/cmake/slang_utils.cmake)

# Find or set slangc executable
find_program(SLANGC_EXECUTABLE slangc
        PATHS
        /usr/local/bin
        /opt/homebrew/bin
        ${CMAKE_SOURCE_DIR}/tools
        REQUIRED
)

add_executable(example_gamegine main.cpp)
target_link_libraries(example_gamegine PRIVATE eng glfw)

# Add shader compilation target
add_slang_shader_target(shader_compile
        SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.slang
)

# Make the example depend on shader compilation
add_dependencies(example_gamegine shader_compile)

# Copy textures directory to output directory so the executable can find them.
# Using copy_directory ensures new .png files are picked up without re-running cmake.
add_custom_command(TARGET example_gamegine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/textures"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/textures"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/textures"
        COMMENT "Copying textures..."
)

# Copy entire models directory so the executable can find them.
# Using copy_directory ensures new .obj files are picked up without re-running cmake.
add_custom_command(TARGET example_gamegine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models"
        COMMENT "Copying models..."
)