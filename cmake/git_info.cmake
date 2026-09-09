# git_info.cmake
#
# Capture the current git state into the build information header. Run in
# script mode from a build-time custom command, so that the commit recorded
# in a benchmark row is the one that produced the binary, and not the one
# that happened to be checked out when CMake last configured.
#
# Expected variables: SRC, TEMPLATE, OUTPUT, ENG_VERSION, ENG_BUILD_TYPE and
# ENG_COMPILER.

execute_process(
        COMMAND git -C "${SRC}" rev-parse --short=12 HEAD
        OUTPUT_VARIABLE ENG_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)

if (NOT ENG_GIT_COMMIT)
    set(ENG_GIT_COMMIT "unknown")
endif ()

execute_process(
        COMMAND git -C "${SRC}" rev-parse --abbrev-ref HEAD
        OUTPUT_VARIABLE ENG_GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)

if (NOT ENG_GIT_BRANCH)
    set(ENG_GIT_BRANCH "unknown")
endif ()

# Tracked modifications only: an untracked scratch file says nothing about
# the code that was compiled.
execute_process(
        COMMAND git -C "${SRC}" status --porcelain --untracked-files=no
        OUTPUT_VARIABLE ENG_GIT_STATUS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)

if (ENG_GIT_STATUS STREQUAL "")
    set(ENG_GIT_DIRTY "false")
else ()
    set(ENG_GIT_DIRTY "true")
endif ()

# Write through a temporary file so that an unchanged header keeps its
# timestamp and does not trigger a rebuild of the whole engine.
configure_file("${TEMPLATE}" "${OUTPUT}.tmp" @ONLY)

execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OUTPUT}.tmp" "${OUTPUT}"
)

execute_process(COMMAND ${CMAKE_COMMAND} -E rm -f "${OUTPUT}.tmp")
