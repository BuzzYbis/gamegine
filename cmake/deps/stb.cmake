include_guard()

include(FetchContent)

FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master
)

FetchContent_MakeAvailable(stb)

# Create INTERFACE library if it doesn't already exist
if(NOT TARGET stb AND NOT TARGET stb::stb)
    add_library(stb INTERFACE)
    add_library(stb::stb ALIAS stb)
    target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
endif()
