include(FetchContent)

FetchContent_Declare(fpattern
    GIT_REPOSITORY "https://github.com/fallout2-ce/fpattern"
    GIT_TAG 35005aec7dd94584a7563fb75bc0597b2259058d # v1.9
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(fpattern)
