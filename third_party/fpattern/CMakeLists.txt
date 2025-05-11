include(FetchContent)

FetchContent_Declare(fpattern
    GIT_REPOSITORY "https://github.com/fallout2-ce/fpattern"
    GIT_TAG f4dd8106c53f5b0f267cc2a16573f2fb203e4a5a # v1.9
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(fpattern)
