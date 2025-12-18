cmake_minimum_required(VERSION 4.1)
project(core)

set(CMAKE_CXX_STANDARD 23)

add_library(core STATIC library.cpp)

target_include_directories(core
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
