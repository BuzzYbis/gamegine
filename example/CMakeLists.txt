include(${GAMEGINE_ROOT}/cmake/deps/glfw.cmake)
include(${GAMEGINE_ROOT}/cmake/deps/vulkan_headers.cmake)


add_executable(gamegine_example main.cpp)
target_link_libraries(gamegine_example PRIVATE glfw Vulkan::Headers)