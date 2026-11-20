cmake_minimum_required(VERSION 3.16)

project(fpattern
    LANGUAGES C
    VERSION "1.0.0"
)

add_library(fpattern-static src/fpattern.c)
add_library(fpattern::fpattern ALIAS fpattern-static)

if(NOT WIN32)
    target_compile_definitions(fpattern-static PRIVATE
        unix=1
    )
endif()

target_include_directories(fpattern-static PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_library(fpattern-windows-static src/fpattern.c)
add_library(fpattern_windows::fpattern_windows ALIAS fpattern-windows-static)

target_compile_definitions(fpattern-windows-static PRIVATE
    FPAT_WINDOWS_PATHS
    fpattern_isvalid=fpattern_windows_isvalid
    fpattern_match=fpattern_windows_match
    fpattern_matchn=fpattern_windows_matchn
)

target_include_directories(fpattern-windows-static PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
