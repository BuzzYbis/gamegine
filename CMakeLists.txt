cmake_minimum_required(VERSION 3.20)
project(gamegine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(GAMEGINE_ROOT ${CMAKE_CURRENT_LIST_DIR})
add_subdirectory(core)
add_subdirectory(example)

add_custom_target(engine)
add_dependencies(engine core)

add_custom_target(example)
add_dependencies(example engine gamegine_example)