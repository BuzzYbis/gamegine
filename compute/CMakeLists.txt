cmake_minimum_required(VERSION 4.1)
project(compute CUDA)

set(CMAKE_CUDA_STANDARD 23)

add_library(compute STATIC library.cu)

set_target_properties(compute PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON)
