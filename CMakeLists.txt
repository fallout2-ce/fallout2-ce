cmake_minimum_required(VERSION 3.18)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(EXECUTABLE_NAME fallout2-ce)

if(APPLE)
    if(IOS)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "12" CACHE STRING "")
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")
    else()
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
    endif()
endif()

project(${EXECUTABLE_NAME})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/EHsc>)
endif()

option(FALLOUT_VENDORED "Use vendored third-party libraries" ON)
option(FALLOUT_AUDIO_OGG "Enable OGG decoding with stb_vorbis when available" ON)

set(FALLOUT_STB_VORBIS_DIR "" CACHE PATH "Directory containing stb_vorbis.c")

if(ANDROID)
    add_library(${EXECUTABLE_NAME} SHARED)
else()
    add_executable(${EXECUTABLE_NAME} WIN32 MACOSX_BUNDLE)
endif()

# Git Info
include("cmake/gitver.cmake")

# Shared engine sources — used by both game and mapper targets.
# Excludes main.cc/main.h (game-only entry logic).
set(FALLOUT_ENGINE_SOURCES
    "src/actions.cc"
    "src/actions.h"
    "src/animation.cc"
    "src/animation.h"
    "src/art.cc"
    "src/art.h"
    "src/audio_file.cc"
    "src/audio_file.h"
    "src/audio.cc"
    "src/audio.h"
    "src/automap.cc"
    "src/automap.h"
    "src/autorun.cc"
    "src/autorun.h"
    "src/cache.cc"
    "src/cache.h"
    "src/character_editor.cc"
    "src/character_editor.h"
    "src/character_selector.cc"
    "src/character_selector.h"
    "src/color.cc"
    "src/color.h"
    "src/combat_ai_defs.h"
    "src/combat_ai.cc"
    "src/combat_ai.h"
    "src/combat_defs.h"
    "src/combat.cc"
    "src/combat.h"
    "src/config.cc"
    "src/config.h"
    "src/content_config.cc"
    "src/content_config.h"
    "src/credits.cc"
    "src/credits.h"
    "src/critter.cc"
    "src/critter.h"
    "src/cycle.cc"
    "src/cycle.h"
    "src/datafile.cc"
    "src/datafile.h"
    "src/db.cc"
    "src/db.h"
    "src/dbox.cc"
    "src/dbox.h"
    "src/debug.cc"
    "src/debug.h"
    "src/dfile.cc"
    "src/dfile.h"
    "src/dialog.cc"
    "src/dialog.h"
    "src/dictionary.cc"
    "src/dictionary.h"
    "src/dinput.cc"
    "src/dinput.h"
    "src/display_monitor.cc"
    "src/display_monitor.h"
    "src/draw.cc"
    "src/draw.h"
    "src/elevator.cc"
    "src/elevator.h"
    "src/endgame.cc"
    "src/endgame.h"
    "src/export.cc"
    "src/export.h"
    "src/file_find.cc"
    "src/file_find.h"
    "src/file_utils.cc"
    "src/file_utils.h"
    "src/font_manager.cc"
    "src/font_manager.h"
    "src/game_config.cc"
    "src/game_config.h"
    "src/game_config_migration.cc"
    "src/game_config_migration.h"
    "src/game_dialog.cc"
    "src/game_dialog.h"
    "src/game_memory.cc"
    "src/game_memory.h"
    "src/game_mouse.cc"
    "src/game_mouse.h"
    "src/game_movie.cc"
    "src/game_movie.h"
    "src/game_sound.cc"
    "src/game_sound.h"
    "src/game_vars.h"
    "src/game.cc"
    "src/game.h"
    "src/geometry.cc"
    "src/geometry.h"
    "src/graph_lib.cc"
    "src/graph_lib.h"
    "src/heap.cc"
    "src/heap.h"
    "src/input.cc"
    "src/input.h"
    "src/interface.cc"
    "src/interface.h"
    "src/interpreter_extra.cc"
    "src/interpreter_extra.h"
    "src/interpreter_lib.cc"
    "src/interpreter_lib.h"
    "src/interpreter.cc"
    "src/interpreter.h"
    "src/inventory.cc"
    "src/inventory.h"
    "src/item.cc"
    "src/item.h"
    "src/kb.cc"
    "src/kb.h"
    "src/light.cc"
    "src/light.h"
    "src/lips.cc"
    "src/lips.h"
    "src/loadsave.cc"
    "src/loadsave.h"
    "src/mainmenu.cc"
    "src/mainmenu.h"
    "src/map_defs.h"
    "src/map.cc"
    "src/map.h"
    "src/memory_defs.h"
    "src/memory_manager.cc"
    "src/memory_manager.h"
    "src/memory.cc"
    "src/memory.h"
    "src/message.cc"
    "src/message.h"
    "src/mouse_manager.cc"
    "src/mouse_manager.h"
    "src/mouse.cc"
    "src/mouse.h"
    "src/movie_effect.cc"
    "src/movie_effect.h"
    "src/movie_lib.cc"
    "src/movie_lib.h"
    "src/movie.cc"
    "src/movie.h"
    "src/nevs.cc"
    "src/nevs.h"
    "src/obj_types.h"
    "src/object.cc"
    "src/object.h"
    "src/ogg_decoder.cc"
    "src/ogg_decoder.h"
    "src/opcode_context.cc"
    "src/opcode_context.h"
    "src/options.cc"
    "src/options.h"
    "src/palette.cc"
    "src/palette.h"
    "src/party_member.cc"
    "src/party_member.h"
    "src/pcx.cc"
    "src/pcx.h"
    "src/perk_defs.h"
    "src/perk.cc"
    "src/perk.h"
    "src/pipboy.cc"
    "src/pipboy.h"
    "src/proto_instance.cc"
    "src/proto_instance.h"
    "src/proto_types.h"
    "src/proto.cc"
    "src/proto.h"
    "src/queue.cc"
    "src/queue.h"
    "src/random.cc"
    "src/random.h"
    "src/reaction.cc"
    "src/reaction.h"
    "src/region.cc"
    "src/region.h"
    "src/scripts.cc"
    "src/scripts.h"
    "src/select_file_list.cc"
    "src/select_file_list.h"
    "src/skill_defs.h"
    "src/skill.cc"
    "src/skill.h"
    "src/skilldex.cc"
    "src/skilldex.h"
    "src/sound_decoder.cc"
    "src/sound_decoder.h"
    "src/sound_effects_cache.cc"
    "src/sound_effects_cache.h"
    "src/sound_effects_list.cc"
    "src/sound_effects_list.h"
    "src/sound.cc"
    "src/sound.h"
    "src/stat_defs.h"
    "src/stat.cc"
    "src/stat.h"
    "src/string_parsers.cc"
    "src/string_parsers.h"
    "src/svga.cc"
    "src/svga.h"
    "src/text_font.cc"
    "src/text_font.h"
    "src/text_object.cc"
    "src/text_object.h"
    "src/tile.cc"
    "src/tile.h"
    "src/tile_hires_stencil.cc"
    "src/tile_hires_stencil.h"
    "src/trait_defs.h"
    "src/trait.cc"
    "src/trait.h"
    "src/version.cc"
    "src/version.h"
    "src/widget.cc"
    "src/widget.h"
    "src/win32.cc"
    "src/win32.h"
    "src/window_manager_private.cc"
    "src/window_manager_private.h"
    "src/window_manager.cc"
    "src/window_manager.h"
    "src/window.cc"
    "src/window.h"
    "src/word_wrap.cc"
    "src/word_wrap.h"
    "src/worldmap.cc"
    "src/worldmap.h"
    "src/xfile.cc"
    "src/xfile.h"
)

set(FALLOUT_PLATFORM_SOURCES
    "src/audio_engine.cc"
    "src/audio_engine.h"
    "src/delay.cc"
    "src/delay.h"
    "src/fps_limiter.cc"
    "src/fps_limiter.h"
    "src/platform_compat.cc"
    "src/platform_compat.h"
    "src/pointer_registry.cc"
    "src/pointer_registry.h"
    "src/preferences.cc"
    "src/preferences.h"
    "src/settings.cc"
    "src/settings.h"
    "src/sfall_config.cc"
    "src/sfall_config.h"
    "src/sfall_ext.cc"
    "src/sfall_ext.h"
    "src/sfall_global_vars.cc"
    "src/sfall_global_vars.h"
    "src/sfall_global_scripts.cc"
    "src/sfall_global_scripts.h"
    "src/sfall_ini.cc"
    "src/sfall_ini.h"
    "src/sfall_kb_helpers.cc"
    "src/sfall_kb_helpers.h"
    "src/sfall_lists.cc"
    "src/sfall_lists.h"
    "src/sfall_metarules.cc"
    "src/sfall_metarules.h"
    "src/sfall_opcodes.cc"
    "src/sfall_opcodes.h"
    "src/sfall_script_hooks.cc"
    "src/script_sound.cc"
    "src/script_sound.h"
    "src/sfall_arrays.cc"
    "src/sfall_arrays.h"
    "src/sfall_animation.cc"
    "src/sfall_animation.h"
    "src/sfall_callbacks.cc"
    "src/sfall_callbacks.h"
    "src/touch.cc"
    "src/touch.h"
    "src/scan_unimplemented.cc"
	"third_party/lodepng/lodepng.cpp"
)

target_sources(${EXECUTABLE_NAME} PUBLIC
    ${FALLOUT_ENGINE_SOURCES}
    "src/main.cc"
    "src/main.h"
)

target_sources(${EXECUTABLE_NAME} PUBLIC ${FALLOUT_PLATFORM_SOURCES})

target_include_directories(${EXECUTABLE_NAME} PUBLIC "third_party/lodepng")

set(FALLOUT_HAVE_STB_VORBIS OFF)
if(FALLOUT_AUDIO_OGG)
    if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/stb_vorbis/stb_vorbis.c")
        set(_fallout_stb_vorbis_dir "${CMAKE_SOURCE_DIR}/third_party/stb_vorbis")
    else()
        set(_fallout_stb_vorbis_dir "${FALLOUT_STB_VORBIS_DIR}")
        if(NOT _fallout_stb_vorbis_dir)
            find_path(_fallout_stb_vorbis_dir
                NAMES "stb_vorbis.c"
                DOC "Directory containing stb_vorbis.c")
        endif()
    endif()

    if(_fallout_stb_vorbis_dir AND EXISTS "${_fallout_stb_vorbis_dir}/stb_vorbis.c")
        set(FALLOUT_HAVE_STB_VORBIS ON)
        target_include_directories(${EXECUTABLE_NAME} PRIVATE "${_fallout_stb_vorbis_dir}")
    else()
        message(STATUS "OGG support disabled: stb_vorbis.c not found")
    endif()
endif()

if(FALLOUT_HAVE_STB_VORBIS)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE HAVE_STB_VORBIS=1)
else()
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE HAVE_STB_VORBIS=0)
endif()

if(IOS)
    target_sources(${EXECUTABLE_NAME} PUBLIC
        "src/platform/ios/paths.h"
        "src/platform/ios/paths.mm"
        "src/platform/ios/quick_toolbar.h"
        "src/platform/ios/quick_toolbar.cc"
    )
endif()

if(WIN32)
    target_compile_definitions(${EXECUTABLE_NAME} PUBLIC
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _STATIC_CPPLIB
    )
endif()

if(WIN32)
    target_link_libraries(${EXECUTABLE_NAME}
        winmm
        debug libcpmtd
        optimized libcpmt
    )
endif()

if(WIN32)
    target_sources(${EXECUTABLE_NAME} PUBLIC
        "os/windows/fallout2-ce.ico"
        "os/windows/fallout2-ce.rc"
    )
endif()

if(APPLE)
    if(IOS)
        set(RESOURCES
            "os/ios/AppIcon.xcassets"
            "os/ios/LaunchScreen.storyboard"
        )

        target_sources(${EXECUTABLE_NAME} PUBLIC ${RESOURCES})
        set_source_files_properties(${RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

        set_target_properties(${EXECUTABLE_NAME} PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/os/ios/Info.plist"
            XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.alexbatalov.fallout2-ce"
            XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        )

        set(MACOSX_BUNDLE_BUNDLE_NAME "${EXECUTABLE_NAME}")
        set(MACOSX_BUNDLE_DISPLAY_NAME "Fallout 2")
    else()
        set(RESOURCES
            "os/macos/fallout2-ce.icns"
        )

        target_link_libraries(${EXECUTABLE_NAME} "-framework CoreFoundation")

        target_sources(${EXECUTABLE_NAME} PUBLIC ${RESOURCES})
        set_source_files_properties(${RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

        set_target_properties(${EXECUTABLE_NAME} PROPERTIES
            OUTPUT_NAME "Fallout II Community Edition"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/os/macos/Info.plist"
            XCODE_ATTRIBUTE_EXECUTABLE_NAME "${EXECUTABLE_NAME}"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.alexbatalov.fallout2-ce"
        )

        set(MACOSX_BUNDLE_ICON_FILE "fallout2-ce.icns")
        set(MACOSX_BUNDLE_BUNDLE_NAME "Fallout II: Community Edition")
        set(MACOSX_BUNDLE_DISPLAY_NAME "Fallout II")
    endif()

    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.alexbatalov.fallout2-ce")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "1.3.0")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "1.3.0")
endif()

if((NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten"))
    if(FALLOUT_VENDORED)
        add_subdirectory("third_party/zlib")
        add_subdirectory("third_party/sdl2")
    else()
        find_package(ZLIB)
        find_package(SDL2)
    endif()
else()
    set( CMAKE_C_FLAGS                      "${CMAKE_C_FLAGS} \
                                            --use-port=sdl2 \
                                            --use-port=sdl2_mixer \
                                            --use-port=zlib" )
    set( CMAKE_CXX_FLAGS                    "${CMAKE_CXX_FLAGS} \
                                            --use-port=sdl2 \
                                            --use-port=sdl2_mixer \
                                            --use-port=zlib \
                                            -std=c++17" )


    set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")
    set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")

    set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O1")
    set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O1")

    if(OPTION_WASM_64)
        target_link_options( ${EXECUTABLE_NAME} PRIVATE "-sMEMORY64" )
        set( CMAKE_C_FLAGS                      "${CMAKE_C_FLAGS} -sMEMORY64" )
        set( CMAKE_CXX_FLAGS                    "${CMAKE_CXX_FLAGS} -sMEMORY64" )
        add_compile_options( -sMEMORY64 )
    endif()

    if(OPTION_WASM_JSPI)
        target_link_options( ${EXECUTABLE_NAME} PRIVATE "-sJSPI" )
    else()
        target_link_options( ${EXECUTABLE_NAME} PRIVATE "-sASYNCIFY" )
        target_link_options( ${EXECUTABLE_NAME} PRIVATE "-sASYNCIFY_STACK_SIZE=20480" )
    endif()

    target_link_options( ${EXECUTABLE_NAME} PRIVATE "-sENVIRONMENT=web" )
    target_link_options( ${EXECUTABLE_NAME} PRIVATE "-sALLOW_MEMORY_GROWTH" )
    target_link_options( ${EXECUTABLE_NAME} PRIVATE "-MEMORY_GROWTH_LINEAR_STEP=32mb" )
    target_link_options( ${EXECUTABLE_NAME} PRIVATE "-sNO_DISABLE_EXCEPTION_CATCHING" )
    target_link_options( ${EXECUTABLE_NAME} PRIVATE "-lidbfs.js" )
    target_link_options( ${EXECUTABLE_NAME} PRIVATE "-sCASE_INSENSITIVE_FS" )
    target_link_options( ${EXECUTABLE_NAME} PRIVATE "-sMODULARIZE" )
    target_link_options( ${EXECUTABLE_NAME} PRIVATE "-sEXPORTED_RUNTIME_METHODS=callMain,addRunDependency,removeRunDependency" )
    target_link_options( ${EXECUTABLE_NAME} PRIVATE "-sEXPORT_ALL" )
    target_link_options( ${EXECUTABLE_NAME} PRIVATE "-sEXPORT_NAME=fallout2ce" )
    target_link_options( ${EXECUTABLE_NAME} PRIVATE "-sEXIT_RUNTIME" )
    target_link_options( ${EXECUTABLE_NAME} PRIVATE "$<$<CONFIG:DEBUG>:-g>" )
endif()


add_subdirectory("third_party/fpattern")
target_link_libraries(${EXECUTABLE_NAME} fpattern::fpattern)
target_link_libraries(${EXECUTABLE_NAME} fpattern_windows::fpattern_windows)

target_link_libraries(${EXECUTABLE_NAME} ${ZLIB_LIBRARIES})
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${ZLIB_INCLUDE_DIRS})

target_link_libraries(${EXECUTABLE_NAME} ${SDL2_LIBRARIES})
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})

if((NOT ANDROID) AND (NOT IOS) AND (NOT CMAKE_SYSTEM_NAME MATCHES "Emscripten"))
    add_executable(ce-dat-tool
        "tools/dat_tool.cc"
        "tools/dat_archive.cc"
        "tools/fo1_dat_lzss.cc"
        "src/dfile.cc"
        "src/platform_compat.cc"
    )

    if(APPLE)
        set_target_properties(ce-dat-tool PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
            XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
        )
    endif()

    target_link_libraries(ce-dat-tool
        fpattern_windows::fpattern_windows
        ${ZLIB_LIBRARIES}
        ${SDL2_LIBRARIES}
    )
    if(WIN32)
        target_link_libraries(ce-dat-tool winmm)
    endif()
    target_include_directories(ce-dat-tool PRIVATE "src")
    target_include_directories(ce-dat-tool PRIVATE ${ZLIB_INCLUDE_DIRS})
    target_include_directories(ce-dat-tool PRIVATE ${SDL2_INCLUDE_DIRS})

    add_executable(ce-frm2png
        "tools/ce_frm2png.cc"
        "src/platform_compat.cc"
        "third_party/lodepng/lodepng.cpp"
    )

    if(APPLE)
        set_target_properties(ce-frm2png PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
            XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
        )
    endif()

    target_include_directories(ce-frm2png PRIVATE "src")
    target_include_directories(ce-frm2png PRIVATE "third_party/lodepng")
    target_include_directories(ce-frm2png PRIVATE ${SDL2_INCLUDE_DIRS})
    target_include_directories(ce-frm2png PRIVATE ${ZLIB_INCLUDE_DIRS})
    target_link_libraries(ce-frm2png ${SDL2_LIBRARIES})
    target_link_libraries(ce-frm2png ${ZLIB_LIBRARIES})

    if(APPLE)
        file(GLOB_RECURSE CE_DAT_INPUTS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/files/ce.dat/*")
        set(CE_DAT_RESOURCE_PATH "${CMAKE_CURRENT_BINARY_DIR}/ce.dat")

        add_custom_command(
            OUTPUT "${CE_DAT_RESOURCE_PATH}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
            COMMAND $<TARGET_FILE:ce-dat-tool> create "${CMAKE_SOURCE_DIR}/files/ce.dat" "${CE_DAT_RESOURCE_PATH}"
            DEPENDS ce-dat-tool ${CE_DAT_INPUTS}
            COMMENT "Generating ce.dat for app bundle"
            VERBATIM
        )

        add_custom_target(ce-dat-resource DEPENDS "${CE_DAT_RESOURCE_PATH}")
        add_dependencies(${EXECUTABLE_NAME} ce-dat-resource)

        target_sources(${EXECUTABLE_NAME} PRIVATE "${CE_DAT_RESOURCE_PATH}")
        set_source_files_properties("${CE_DAT_RESOURCE_PATH}" PROPERTIES
            GENERATED TRUE
            MACOSX_PACKAGE_LOCATION "Resources")
    endif()
endif()

if((NOT ANDROID) AND (NOT IOS) AND (NOT CMAKE_SYSTEM_NAME MATCHES "Emscripten"))
    if(WIN32)
        add_executable(mapper-ce WIN32)
    else()
        add_executable(mapper-ce)
    endif()

    target_sources(mapper-ce PUBLIC
        ${FALLOUT_ENGINE_SOURCES}
        ${FALLOUT_PLATFORM_SOURCES}
        "src/mapper/map_func.cc"
        "src/mapper/map_func.h"
        "src/mapper/mapper.cc"
        "src/mapper/mapper.h"
        "src/mapper/mp_instance.cc"
        "src/mapper/mp_instance.h"
        "src/mapper/mp_proto.cc"
        "src/mapper/mp_proto.h"
        "src/mapper/mp_scrpt.cc"
        "src/mapper/mp_scrpt.h"
        "src/mapper/mp_targt.cc"
        "src/mapper/mp_targt.h"
        "src/mapper/mp_text.cc"
        "src/mapper/mp_text.h"
    )

    target_compile_definitions(mapper-ce PUBLIC FALLOUT_MAPPER)

    if(WIN32)
        target_compile_definitions(mapper-ce PUBLIC
            _CRT_SECURE_NO_WARNINGS
            _CRT_NONSTDC_NO_WARNINGS
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            _STATIC_CPPLIB
        )
        target_link_libraries(mapper-ce
            winmm
            debug libcpmtd
            optimized libcpmt
        )
        target_sources(mapper-ce PUBLIC
            "os/windows/fallout2-ce.rc"
        )
    endif()

    if(APPLE)
        target_link_libraries(mapper-ce "-framework CoreFoundation")
        set_target_properties(mapper-ce PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
            XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
        )
    endif()

    target_include_directories(mapper-ce PUBLIC "third_party/lodepng")
    target_include_directories(mapper-ce PRIVATE "src")
    target_include_directories(mapper-ce PRIVATE ${ZLIB_INCLUDE_DIRS})
    target_include_directories(mapper-ce PRIVATE ${SDL2_INCLUDE_DIRS})

    target_link_libraries(mapper-ce
        fpattern::fpattern
        fpattern_windows::fpattern_windows
        ${ZLIB_LIBRARIES}
        ${SDL2_LIBRARIES}
    )
endif()

if(APPLE)
    if(IOS)
        install(TARGETS ${EXECUTABLE_NAME} DESTINATION "Payload")

        set(CPACK_GENERATOR "ZIP")
        set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
        set(CPACK_PACKAGE_FILE_NAME "fallout2-ce")
        set(CPACK_ARCHIVE_FILE_EXTENSION "ipa")
    else()
        install(TARGETS ${EXECUTABLE_NAME} DESTINATION .)

        set(CPACK_GENERATOR "DragNDrop")
        set(CPACK_DMG_DISABLE_APPLICATIONS_SYMLINK ON)
        set(CPACK_PACKAGE_FILE_NAME "Fallout II Community Edition")
    endif()

    include(CPack)
endif()

# Add option to enable AddressSanitizer
option(ENABLE_ASAN "Enable AddressSanitizer for debug builds" OFF)

# Set C++ standard and other existing settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

# Define ASan flags for supported compilers (GCC, Clang, MSVC)
if(ENABLE_ASAN)
    if(CMAKE_SYSTEM_NAME MATCHES "Emscripten|Android|iOS")
        message(WARNING "AddressSanitizer is not supported on ${CMAKE_SYSTEM_NAME}. Disabling ASan.")
        set(ENABLE_ASAN OFF)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Enabling AddressSanitizer for debug build (GCC/Clang)")
        set(ASAN_FLAGS "-fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${ASAN_FLAGS}")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${ASAN_FLAGS}")
        set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${ASAN_FLAGS}")
        set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} ${ASAN_FLAGS}")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        # Check if using clang-cl or native MSVC
        if(CMAKE_CXX_COMPILER MATCHES "clang-cl")
            message(STATUS "Enabling AddressSanitizer for debug build (clang-cl)")
            set(ASAN_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
            set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${ASAN_FLAGS}")
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${ASAN_FLAGS}")
            set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address")
            set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=address")
        else()
            message(STATUS "Enabling AddressSanitizer for debug build (MSVC)")
            set(ASAN_FLAGS "/fsanitize=address")
            # MSVC requires /Zi or /Z7 for debug info with ASan
            set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${ASAN_FLAGS} /Zi")
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${ASAN_FLAGS} /Zi")
            # No additional linker flags needed for MSVC ASan
            # Disable incremental linking as it's incompatible with ASan
            string(REPLACE "/INCREMENTAL" "/INCREMENTAL:NO" CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG}")
            string(REPLACE "/INCREMENTAL" "/INCREMENTAL:NO" CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG}")
        endif()
    else()
        message(WARNING "AddressSanitizer is not supported with ${CMAKE_CXX_COMPILER_ID}. Disabling ASan.")
        set(ENABLE_ASAN OFF)
    endif()
endif()

# Existing debug flags for Emscripten (ensure ASan doesn't conflict)
if(CMAKE_SYSTEM_NAME MATCHES "Emscripten")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
endif()

# Optional path to which the built executable and PDB will be copied after build.
# Can be set via -DCOPY_TO_PATH=/some/path or through the CMake/CLion GUI.
set(COPY_TO_PATH "" CACHE PATH "Destination directory for copying the executable and PDB after build")

if(DEFINED COPY_TO_PATH AND NOT "${COPY_TO_PATH}" STREQUAL "")
    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${COPY_TO_PATH}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${EXECUTABLE_NAME}>"
            "${COPY_TO_PATH}/"
        COMMAND ${CMAKE_COMMAND} -E echo "Copied executable to ${COPY_TO_PATH}"
    )
    if(MSVC)
        add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_PDB_FILE:${EXECUTABLE_NAME}>"
                "${COPY_TO_PATH}/"
                COMMAND ${CMAKE_COMMAND} -E echo "Copied PDB to ${COPY_TO_PATH}"
        )
        message(STATUS "Will copy executable and PDB to: ${COPY_TO_PATH}")
    else()
        message(STATUS "Will copy executable to: ${COPY_TO_PATH}")
    endif()
endif()
