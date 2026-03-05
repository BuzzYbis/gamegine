include_guard(GLOBAL)
include(FetchContent)

FetchContent_Declare(entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG v3.11.0 # Using a recent stable version
)

FetchContent_MakeAvailable(entt)
