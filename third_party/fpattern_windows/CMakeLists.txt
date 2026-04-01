cmake_minimum_required(VERSION 3.16)

project(fpattern_windows
    LANGUAGES C
    VERSION "1.0.0"
)

add_library(fpattern_windows-static src/fpattern.c)
add_library(fpattern_windows::fpattern_windows ALIAS fpattern_windows-static)

target_compile_definitions(fpattern_windows-static PRIVATE
    FPAT_WINDOWS_PATHS
    fpattern_isvalid=fpattern_windows_isvalid
    fpattern_match=fpattern_windows_match
    fpattern_matchn=fpattern_windows_matchn
)

target_include_directories(fpattern_windows-static PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
