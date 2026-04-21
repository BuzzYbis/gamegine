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

# Copy textures directory to output directory so the executable can find them
file(GLOB_RECURSE TEXTURE_FILES
        "assets/textures/*"
)

foreach (TEXTURE_FILE ${TEXTURE_FILES})
    # Get relative path from textures directory
    file(RELATIVE_PATH RELATIVE_PATH
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/textures"
            "${TEXTURE_FILE}")
    # Set destination path in binary directory
    set(DEST_PATH "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/textures/${RELATIVE_PATH}")
    # Add custom command to copy the file
    add_custom_command(TARGET example_gamegine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/textures"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TEXTURE_FILE}" "${DEST_PATH}"
            COMMENT "Copying texture: ${RELATIVE_PATH}"
    )
endforeach ()

# Copy entire models directory so the executable can find them.
# Using copy_directory ensures new .obj files are picked up without re-running cmake.
add_custom_command(TARGET example_gamegine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models"
        COMMENT "Copying models..."
)