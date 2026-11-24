# ----------------------------------------------------------------------------------
# CannonBall DX CMake File
#
# CannonBall DX is based on CannonBall-SE by James Pearce
# CannonBall-SE revisions Copyright (c) James Pearce, 2025
#
# CannonBall is Copyright (c) Chris White, see: https://github.com/djyt/cannonball
#
# Tested on Linux (Ubuntu on Intel and Raspbian) and Windows 11
# ----------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.20)
project(cannonball-dx)

# iOS and visionOS builds cross-compile with -G Xcode and
# -DCMAKE_SYSTEM_NAME=iOS or visionOS (see docs/Compiling-On-iOS.txt and
# docs/Compiling-On-visionOS.txt).
set(CB_IOS FALSE)
set(CB_VISIONOS FALSE)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(CB_IOS TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "visionOS")
    set(CB_VISIONOS TRUE)
endif()
if(CB_IOS OR CB_VISIONOS)
    set(CB_APPLE_MOBILE TRUE)
    # "xcodebuild archive" must produce an app archive that -exportArchive can
    # turn into an .ipa: nothing but the app gets installed into it (the app
    # target opts back in below). Without this the archive is "generic".
    set(CMAKE_XCODE_ATTRIBUTE_SKIP_INSTALL YES)
else()
    set(CB_APPLE_MOBILE FALSE)
endif()
include(GNUInstallDirs)
include(FetchContent)

# --- Compiler version checks ------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12)
    message(FATAL_ERROR "C++23 support requires GCC 12+")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15)
    message(FATAL_ERROR "C++23 support requires Clang 15+")
elseif(MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.36)
    message(FATAL_ERROR "C++23 support requires MSVC 19.36+ (Visual Studio 2022 17.6+)")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# --- Build verbosity controls --------------------------------------------------
option(QUIET_BUILD "Reduce build output & warning noise (recommended for users)" ON)
option(SUPER_QUIET "Silence *all* compiler warnings (-w) in Release-like configs" OFF)

# Keep build logs tidy (affects Makefiles/Unix Makefiles; Ninja is already terse)
set(CMAKE_VERBOSE_MAKEFILE OFF CACHE BOOL "Do not echo full compile/link commands")
if(QUIET_BUILD)
  # Hide per-target boilerplate like "Building CXX object ..."
  set(CMAKE_RULE_MESSAGES OFF CACHE BOOL "Suppress CMake rule messages" FORCE)
endif()

# Default to Release if nothing is specified (single-config generators)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No build type specified; defaulting to Release")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
               "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# For multi-config generators (Visual Studio / Xcode)
if(CMAKE_CONFIGURATION_TYPES)
  # Generate these configs; put Release first
  set(CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo;MinSizeRel;Debug"
      CACHE STRING "" FORCE)
  # Make 'Release' the default for `cmake --install` when --config is omitted
  set(CMAKE_INSTALL_CONFIG_NAME Release CACHE STRING "" FORCE)
endif()

# --- Optimisation level --------------------------------------------------------
option(WITH_MARCH_NATIVE "Build with -march=native" ON)

# --- Profiling toggles ---------------------------------------------------------
option(ENABLE_PROFILING        "Turn on common profiling flags" OFF)
option(PROFILE_WITH_PERF       "Tweak flags for Linux perf (symbols + frame pointers)" OFF)
option(PROFILE_WITH_GPROF      "Build with -pg for gprof" OFF)
option(PROFILE_WITH_CALLGRIND  "Reduce inlining for cleaner callgrind graphs" OFF)

# --- Pixel accuracy mode ---
option(PIXEL_ACCURACY "Enable pixel-accurate sprite rendering" ON)

# --- Option for MP3 playback ---
if(CB_APPLE_MOBILE)
    # No mpg123 build for iOS/visionOS yet; WAV and YM custom music still work.
    option(WITH_MP3 "Enable MP3 playback via mpg123" OFF)
else()
    option(WITH_MP3 "Enable MP3 playback via mpg123" ON)
endif()

# --- Option to install man page ---
option(INSTALL_MANPAGE "Install cannonball-dx man page" ON)

# --- Source directory base ---
set(main_base cannonball-dx)
set(main_cpp_base src/main)

# -----------------------------------------------------------------------------
# Optional pixel scaler source used by the Enhancements menu.
# Pull the HqMAME-derived implementations from a pinned Mesen2 snapshot and
# strip Mesen's project-specific precompiled-header include before compiling.
# -----------------------------------------------------------------------------
FetchContent_Declare(
    cannonball_scalers
    GIT_REPOSITORY https://github.com/SourMesen/Mesen2.git
    GIT_TAG b9fa69ddc6d0a331fb103fdb5eef6904305703c2
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(cannonball_scalers)
if(NOT cannonball_scalers_POPULATED)
    FetchContent_Populate(cannonball_scalers)
endif()

set(CB_SCALER_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_scalers")
file(MAKE_DIRECTORY "${CB_SCALER_GEN_DIR}/xbrz")
file(MAKE_DIRECTORY "${CB_SCALER_GEN_DIR}/hqx")

function(cb_prepare_scaler_file source_subdir filename output_subdir)
    file(READ
        "${cannonball_scalers_SOURCE_DIR}/${source_subdir}/${filename}"
        _cb_scaler_content)
    string(REPLACE "#include \"../pch.h\"\r\n" "" _cb_scaler_content "${_cb_scaler_content}")
    string(REPLACE "#include \"../pch.h\"\n" "" _cb_scaler_content "${_cb_scaler_content}")
    file(WRITE
        "${CB_SCALER_GEN_DIR}/${output_subdir}/${filename}"
        "${_cb_scaler_content}")
endfunction()

cb_prepare_scaler_file("Utilities/xBRZ" "config.h" "xbrz")
cb_prepare_scaler_file("Utilities/xBRZ" "xbrz.h" "xbrz")
cb_prepare_scaler_file("Utilities/xBRZ" "xbrz.cpp" "xbrz")
cb_prepare_scaler_file("Utilities/HQX" "common.h" "hqx")
cb_prepare_scaler_file("Utilities/HQX" "hqx.h" "hqx")
cb_prepare_scaler_file("Utilities/HQX" "init.cpp" "hqx")
cb_prepare_scaler_file("Utilities/HQX" "hq2x.cpp" "hqx")
cb_prepare_scaler_file("Utilities/HQX" "hq3x.cpp" "hqx")
cb_prepare_scaler_file("Utilities/HQX" "hq4x.cpp" "hqx")

set(src_scalers
    "${CB_SCALER_GEN_DIR}/xbrz/xbrz.cpp"
    "${CB_SCALER_GEN_DIR}/hqx/init.cpp"
    "${CB_SCALER_GEN_DIR}/hqx/hq2x.cpp"
    "${CB_SCALER_GEN_DIR}/hqx/hq3x.cpp"
    "${CB_SCALER_GEN_DIR}/hqx/hq4x.cpp"
)

# -----------------------------------------------------------------------------
# Project Source Files
# -----------------------------------------------------------------------------

set(src_main
    "${main_cpp_base}/globals.hpp"
    "${main_cpp_base}/romloader.hpp"
    "${main_cpp_base}/roms.hpp"
    "${main_cpp_base}/trackloader.hpp"
    "${main_cpp_base}/stdint.hpp"
    "${main_cpp_base}/main.hpp"
    "${main_cpp_base}/video.hpp"
    "${main_cpp_base}/utils.hpp"
    "${main_cpp_base}/windirent.h"

    "${main_cpp_base}/main.cpp"
    "${main_cpp_base}/romloader.cpp"
    "${main_cpp_base}/trackloader.cpp"
    "${main_cpp_base}/roms.cpp"
    "${main_cpp_base}/video.cpp"
    "${main_cpp_base}/utils.cpp"
    )

set(src_frontend
    "${main_cpp_base}/frontend/cabdiag.hpp"
    "${main_cpp_base}/frontend/config.hpp"
    "${main_cpp_base}/frontend/menu.hpp"
    "${main_cpp_base}/frontend/menulabels.hpp"
    "${main_cpp_base}/frontend/ttrial.hpp"

    "${main_cpp_base}/frontend/cabdiag.cpp"
    "${main_cpp_base}/frontend/config.cpp"
    "${main_cpp_base}/frontend/menu.cpp"
    "${main_cpp_base}/frontend/ttrial.cpp"
    )

set(src_hwvideo
    "${main_cpp_base}/hwvideo/hwroad.hpp"
    "${main_cpp_base}/hwvideo/hwsprites.hpp"
    "${main_cpp_base}/hwvideo/hwtiles.hpp"

    "${main_cpp_base}/hwvideo/hwroad.cpp"
    "${main_cpp_base}/hwvideo/hwsprites.cpp"
    "${main_cpp_base}/hwvideo/hwtiles.cpp"
    )

set(src_hwaudio
    "${main_cpp_base}/hwaudio/segapcm.hpp"
    "${main_cpp_base}/hwaudio/soundchip.hpp"
    "${main_cpp_base}/hwaudio/ym2151.hpp"

    "${main_cpp_base}/hwaudio/segapcm.cpp"
    "${main_cpp_base}/hwaudio/soundchip.cpp"
    "${main_cpp_base}/hwaudio/ym2151.cpp"
    )

set(src_sdl
    "${main_cpp_base}/sdl2/audio.hpp"
    "${main_cpp_base}/sdl2/wav123.hpp"
    "${main_cpp_base}/sdl2/timer.hpp"
    "${main_cpp_base}/sdl2/input.hpp"
    "${main_cpp_base}/sdl2/renderbase.hpp"
    "${main_cpp_base}/sdl2/snes_ntsc.h"
    "${main_cpp_base}/sdl2/pixel_scaler_state.hpp"

    "${main_cpp_base}/sdl2/audio.cpp"
    "${main_cpp_base}/sdl2/wav123.cpp"
    "${main_cpp_base}/sdl2/timer.cpp"
    "${main_cpp_base}/sdl2/input.cpp"
    "${main_cpp_base}/sdl2/renderbase.cpp"
    "${main_cpp_base}/sdl2/snes_ntsc.cpp"
    )

set(src_render
    "${main_cpp_base}/sdl2/rendersurface.hpp"
    "${main_cpp_base}/sdl2/rendersurface.cpp"
    )

set(src_engine
    "${main_cpp_base}/engine/oaddresses.hpp"
    "${main_cpp_base}/engine/oanimseq.hpp"
    "${main_cpp_base}/engine/oanimsprite.hpp"
    "${main_cpp_base}/engine/oattractai.hpp"
    "${main_cpp_base}/engine/obonus.hpp"
    "${main_cpp_base}/engine/ocrash.hpp"
    "${main_cpp_base}/engine/oentry.hpp"
    "${main_cpp_base}/engine/oferrari.hpp"
    "${main_cpp_base}/engine/ohiscore.hpp"
    "${main_cpp_base}/engine/ohud.hpp"
    "${main_cpp_base}/engine/oinitengine.hpp"
    "${main_cpp_base}/engine/oinputs.hpp"
    "${main_cpp_base}/engine/olevelobjs.hpp"
    "${main_cpp_base}/engine/ologo.hpp"
    "${main_cpp_base}/engine/omap.hpp"
    "${main_cpp_base}/engine/omusic.hpp"
    "${main_cpp_base}/engine/ooutputs.hpp"
    "${main_cpp_base}/engine/opalette.hpp"
    "${main_cpp_base}/engine/oroad.hpp"
    "${main_cpp_base}/engine/osmoke.hpp"
    "${main_cpp_base}/engine/osprite.hpp"
    "${main_cpp_base}/engine/osprites.hpp"
    "${main_cpp_base}/engine/ostats.hpp"
    "${main_cpp_base}/engine/otiles.hpp"
    "${main_cpp_base}/engine/otraffic.hpp"
    "${main_cpp_base}/engine/outils.hpp"
    "${main_cpp_base}/engine/outrun.hpp"
    "${main_cpp_base}/engine/oanimseq.cpp"
    "${main_cpp_base}/engine/oattractai.cpp"
    "${main_cpp_base}/engine/obonus.cpp"
    "${main_cpp_base}/engine/ocrash.cpp"
    "${main_cpp_base}/engine/oferrari.cpp"
    "${main_cpp_base}/engine/ohiscore.cpp"
    "${main_cpp_base}/engine/ohud.cpp"
    "${main_cpp_base}/engine/oinitengine.cpp"
    "${main_cpp_base}/engine/oinputs.cpp"
    "${main_cpp_base}/engine/olevelobjs.cpp"
    "${main_cpp_base}/engine/ologo.cpp"
    "${main_cpp_base}/engine/omap.cpp"
    "${main_cpp_base}/engine/omusic.cpp"
    "${main_cpp_base}/engine/ooutputs.cpp"
    "${main_cpp_base}/engine/opalette.cpp"
    "${main_cpp_base}/engine/oroad.cpp"
    "${main_cpp_base}/engine/osmoke.cpp"
    "${main_cpp_base}/engine/osprite.cpp"
    "${main_cpp_base}/engine/osprites.cpp"
    "${main_cpp_base}/engine/ostats.cpp"
    "${main_cpp_base}/engine/otiles.cpp"
    "${main_cpp_base}/engine/otraffic.cpp"
    "${main_cpp_base}/engine/outils.cpp"
    "${main_cpp_base}/engine/outrun.cpp"
    )

set(src_engine_data
    "${main_cpp_base}/engine/data/ozoom_lookup.hpp"
    "${main_cpp_base}/engine/data/sprite_pals.hpp"
    )

set(src_engine_audio
    "${main_cpp_base}/engine/audio/commands.hpp"
    "${main_cpp_base}/engine/audio/osound.hpp"
    "${main_cpp_base}/engine/audio/osoundint.hpp"
    "${main_cpp_base}/engine/audio/osoundadr.hpp"

    "${main_cpp_base}/engine/audio/osound.cpp"
    "${main_cpp_base}/engine/audio/osoundint.cpp"
    )

set(src_directx
    "${main_cpp_base}/directx/ffeedback.hpp"
    "${main_cpp_base}/directx/ffeedback.cpp"
    )

if(WIN32)
    # Embed version metadata in the executable. globals.hpp remains the single
    # source of truth so the version shown by Windows always matches the game.
    file(READ "${main_cpp_base}/globals.hpp" _cb_globals_hpp)

    string(REGEX MATCH "CANNONBALL_DX_VERSION[ \t]+\"([^\"]+)\"" _cb_unused "${_cb_globals_hpp}")
    if(NOT CMAKE_MATCH_1)
        message(FATAL_ERROR
            "Could not parse CANNONBALL_DX_VERSION from globals.hpp. "
            "res/cannonball.rc.in needs it; fix the parse rather than hardcoding a version.")
    endif()
    set(CB_VERSION_STRING "${CMAKE_MATCH_1}")

    # VERSIONINFO requires numeric fields. Accept one, two or three version parts.
    string(REPLACE "." ";" _cb_version_parts "${CB_VERSION_STRING}")
    list(LENGTH _cb_version_parts _cb_version_count)
    list(GET _cb_version_parts 0 CB_VERSION_MAJOR)
    if(_cb_version_count GREATER 1)
        list(GET _cb_version_parts 1 CB_VERSION_MINOR)
    else()
        set(CB_VERSION_MINOR 0)
    endif()
    if(_cb_version_count GREATER 2)
        list(GET _cb_version_parts 2 CB_VERSION_PATCH)
    else()
        set(CB_VERSION_PATCH 0)
    endif()

    # The generated .rc lives in the build tree, so the icon needs an absolute path.
    set(CB_ICON_PATH "${CMAKE_SOURCE_DIR}/res/cannonball.ico")

    configure_file(
        "${CMAKE_SOURCE_DIR}/res/cannonball.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cannonball.rc"
        @ONLY
    )

    message(STATUS "CannonBall DX version ${CB_VERSION_STRING} "
                   "(resource ${CB_VERSION_MAJOR},${CB_VERSION_MINOR},${CB_VERSION_PATCH},0)")

    list(APPEND src_directx
        "${CMAKE_CURRENT_BINARY_DIR}/cannonball.rc"
    )
endif()

set(SRCS
    ${src_main}
    ${src_frontend}
    ${src_hwvideo}
    ${src_hwaudio}
    ${src_sdl}
    ${src_render}
    ${src_scalers}
    ${src_directx}
    ${src_engine}
    ${src_engine_data}
    ${src_engine_audio}
)

# Group files into relevant filters for Visual Studio build
SOURCE_GROUP(src FILES ${src_main})
SOURCE_GROUP(src\\frontend FILES ${src_frontend})
SOURCE_GROUP(src\\hwvideo FILES ${src_hwvideo})
SOURCE_GROUP(src\\hwaudio FILES ${src_hwaudio})
SOURCE_GROUP(src\\sdl FILES ${src_sdl})
SOURCE_GROUP(src\\sdl FILES ${src_render})
SOURCE_GROUP(src\\sdl\\scalers FILES ${src_scalers})
SOURCE_GROUP(src\\directx FILES ${src_directx})
SOURCE_GROUP(src\\engine FILES ${src_engine})
SOURCE_GROUP(src\\engine\\data FILES ${src_engine_data})
SOURCE_GROUP(src\\engine\\audio FILES ${src_engine_audio})

add_executable(cannonball-dx ${SRCS})

# --- ZIP ROM support -----------------------------------------------------------
# miniz is used only for reading ROMs directly from ZIP archives. Keep it pinned
# so Windows, Linux and Raspberry Pi builds all use the same implementation.
set(BUILD_EXAMPLES OFF CACHE BOOL "Build miniz examples" FORCE)
set(BUILD_FUZZERS OFF CACHE BOOL "Build miniz fuzzers" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Build miniz tests" FORCE)
set(INSTALL_PROJECT OFF CACHE BOOL "Install miniz" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build miniz as a static library" FORCE)
FetchContent_Declare(
    cannonball_miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG 77d0dce8627735138c51770d1799a1ef48f2117d
)
FetchContent_MakeAvailable(cannonball_miniz)
target_link_libraries(cannonball-dx PRIVATE miniz)
# miniz enables verbose makefiles in its own project; restore CannonBall's choice.
set(CMAKE_VERBOSE_MAKEFILE OFF CACHE BOOL "Do not echo full compile/link commands" FORCE)

if(WIN32)
    target_compile_features(cannonball-dx PRIVATE cxx_std_23)
    target_compile_definitions(cannonball-dx PRIVATE
        MPG123_NO_LARGENAME      # stop header from mapping foo -> foo_64
        MPG123_NO_64BIT_API
    )
endif()

if(MSVC)
    set_property(TARGET cannonball-dx
        PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()

if(PIXEL_ACCURACY)
    target_compile_definitions(cannonball-dx PRIVATE PIXEL_ACCURACY=1)
else()
    target_compile_definitions(cannonball-dx PRIVATE PIXEL_ACCURACY=0)
endif()

# --- Dependencies: SDL2 (robust handling), TinyXML2, EGL/GLESv2 -------------------

# SDL2: Prefer modern imported targets; fall back to MODULE or pkg-config.
set(_SDL2_TARGETS "")
set(_SDL2_INCLUDE_DIRS "")
set(_SDL2_LINK_TARGET "")

if(CB_IOS)
    # iOS: no system packages, so build pinned SDL2 and tinyxml2 statically.
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        cannonball_sdl2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.32.10
        GIT_SHALLOW TRUE
    )
    set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        cannonball_tinyxml2
        GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
        GIT_TAG 11.0.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(cannonball_sdl2 cannonball_tinyxml2)
    # SDL2main supplies main() and runs our (renamed) SDL_main inside UIKit.
    list(APPEND _SDL2_TARGETS SDL2::SDL2main SDL2::SDL2-static)
    set(_TINYXML2_TARGET tinyxml2::tinyxml2)
elseif(CB_VISIONOS)
    # visionOS: SDL2 does not support it, SDL3 does. Build SDL3 as an embedded
    # framework and compile sdl2-compat (the SDL2 API implemented on SDL3) into
    # the game. sdl2-compat loads SDL3 at runtime, so the two never link
    # together and their SDL_* symbols cannot clash.
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_FRAMEWORK ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        cannonball_sdl3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.20
        GIT_SHALLOW TRUE
    )
    FetchContent_Declare(
        cannonball_sdl2compat
        GIT_REPOSITORY https://github.com/libsdl-org/sdl2-compat.git
        GIT_TAG release-2.32.74
        GIT_SHALLOW TRUE
    )
    set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        cannonball_tinyxml2
        GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
        GIT_TAG 11.0.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(cannonball_sdl3 cannonball_tinyxml2)
    FetchContent_GetProperties(cannonball_sdl2compat)
    if(NOT cannonball_sdl2compat_POPULATED)
        FetchContent_Populate(cannonball_sdl2compat)
    endif()

    # Patch the fetched sdl2-compat sources (idempotent):
    # - its SDL3 search paths follow the macOS bundle layout; add the flat
    #   Frameworks/ layout of iOS-family apps.
    # - its iOS prototype for SDL_iPhoneSetAnimationCallback has unbalanced
    #   parentheses (and lacks the callbackParam argument), so iOS-family
    #   builds do not compile.
    function(cb_patch_file path from to)
        file(READ "${path}" _content)
        string(FIND "${_content}" "${to}" _already)
        if(_already EQUAL -1)
            string(REPLACE "${from}" "${to}" _patched "${_content}")
            if(_patched STREQUAL _content)
                message(FATAL_ERROR "Could not patch ${path}")
            endif()
            file(WRITE "${path}" "${_patched}")
        endif()
    endfunction()
    cb_patch_file("${cannonball_sdl2compat_SOURCE_DIR}/src/sdl2_compat.c"
        "\"@executable_path/../Frameworks/\" SDL3_FRAMEWORK,"
        "\"@executable_path/Frameworks/SDL3.framework/SDL3\", /* iOS-family app bundle */\n            \"@executable_path/../Frameworks/\" SDL3_FRAMEWORK,")
    cb_patch_file("${cannonball_sdl2compat_SOURCE_DIR}/src/sdl2_protos.h"
        "SDL2_PROTO(int,iPhoneSetAnimationCallback,(SDL_Window *a, int b, void (SDLCALL *c)(void *))\n"
        "SDL2_PROTO(int,iPhoneSetAnimationCallback,(SDL_Window *a, int b, void (SDLCALL *c)(void *), void *d))\n")
    # - SetiOSEventPump is listed with the renamed-symbol arguments but the
    #   plain SDL3_SYM macro. SDL_iPhoneSetEventPump is hand-written, so only
    #   the SDL3 function needs loading.
    cb_patch_file("${cannonball_sdl2compat_SOURCE_DIR}/src/sdl3_syms.h"
        "SDL3_SYM(void,iPhoneSetEventPump,SetiOSEventPump,(bool a),(a),)"
        "SDL3_SYM(void,SetiOSEventPump,(bool a),(a),)")
    # - SDL3's SDL_opengl.h is empty on iOS-family targets, but sdl2-compat
    #   still needs the GL type names for its texture-binding helpers.
    cb_patch_file("${cannonball_sdl2compat_SOURCE_DIR}/src/sdl2_compat.c"
        "#include <SDL3/SDL_opengl.h>\n#include <SDL3/SDL_opengl_glext.h>\n"
        "#ifdef SDL_PLATFORM_IOS /* GL types only; there is no GL on iOS-family targets */\n#undef SDL_PLATFORM_IOS\n#include <SDL3/SDL_opengl.h>\n#include <SDL3/SDL_opengl_glext.h>\n#define SDL_PLATFORM_IOS 1\n#else\n#include <SDL3/SDL_opengl.h>\n#include <SDL3/SDL_opengl_glext.h>\n#endif\n")

    enable_language(OBJC)
    add_library(cb_sdl2compat STATIC
        "${cannonball_sdl2compat_SOURCE_DIR}/src/sdl2_compat.c"
        "${cannonball_sdl2compat_SOURCE_DIR}/src/dynapi/SDL_dynapi.c"
        # Its src/sdl2_compat_objc.m is macOS-only (Cocoa); use a UIKit version.
        "${CMAKE_SOURCE_DIR}/platform/visionos/sdl2compat_uikit.m")
    target_include_directories(cb_sdl2compat
        PRIVATE   "${cannonball_sdl2compat_SOURCE_DIR}/src"
        # The SDL2 headers are for the game only; sdl2-compat itself compiles
        # against the SDL3 headers.
        INTERFACE "${cannonball_sdl2compat_SOURCE_DIR}/include"
                  "${cannonball_sdl2compat_SOURCE_DIR}/include/SDL2")
    target_link_libraries(cb_sdl2compat PRIVATE SDL3::Headers)
    target_compile_definitions(cb_sdl2compat PRIVATE _REENTRANT "SDL2COMPAT_REVISION=\"release-2.32.74\"")
    target_compile_options(cb_sdl2compat PRIVATE -w)

    list(APPEND _SDL2_TARGETS cb_sdl2compat)
    set(_TINYXML2_TARGET tinyxml2::tinyxml2)
else()
find_package(SDL2 QUIET CONFIG)
if(TARGET SDL2::SDL2)
    list(APPEND _SDL2_TARGETS SDL2::SDL2)
    if(WIN32 AND TARGET SDL2::SDL2main)
        list(APPEND _SDL2_TARGETS SDL2::SDL2main)
    endif()
else()
    # Try FindSDL2.cmake (MODULE mode) which defines SDL2_INCLUDE_DIRS/SDL2_LIBRARIES
    find_package(SDL2 QUIET MODULE)
    if(SDL2_FOUND)
        set(_SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS})
        set(_SDL2_LINK_TARGET ${SDL2_LIBRARIES})
    else()
        # Fallback: pkg-config
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
        set(_SDL2_LINK_TARGET PkgConfig::SDL2)
    endif()
endif()

# --- TinyXML2 for XML config I/O (replaces Boost.PropertyTree in config.cpp) ---
find_package(TinyXML2 QUIET)
if(NOT TARGET TinyXML2::TinyXML2)
    find_package(tinyxml2 QUIET) # alternate export name on some distros
endif()

if(TARGET TinyXML2::TinyXML2)
    set(_TINYXML2_TARGET TinyXML2::TinyXML2)
elseif(TARGET tinyxml2::tinyxml2)
    set(_TINYXML2_TARGET tinyxml2::tinyxml2)
else()
    # Fall back to pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(TINYXML2 REQUIRED IMPORTED_TARGET tinyxml2)
    set(_TINYXML2_TARGET PkgConfig::TINYXML2)
endif()
endif() # CB_IOS / CB_VISIONOS

# ——————————————————————————————————————
# EGL / OpenGLES2 via pkg-config (Linux, Android, etc)
# ——————————————————————————————————————
# headers used like #include "sdl2/foo.hpp" live under src/main
target_include_directories(cannonball-dx PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src/main"
  "${CB_SCALER_GEN_DIR}/xbrz"
  "${CB_SCALER_GEN_DIR}/hqx"
  ${_SDL2_INCLUDE_DIRS}               # present on all platforms
)

option(APPLE_USE_ANGLE "Apple: render through a GLES2 driver (e.g. ANGLE) instead of the OpenGL framework" OFF)

if(CB_VISIONOS)
  # visionOS: no OpenGL ES. The SDL_Renderer backend (sdlr_backend.hpp) draws
  # with SDL3's Metal renderer.
elseif(CB_IOS)
  # iOS: the system OpenGL ES 2.0 framework (deprecated by Apple but shipped).
  target_link_libraries(cannonball-dx PRIVATE "-framework OpenGLES")
  target_compile_definitions(cannonball-dx PRIVATE GLES_SILENCE_DEPRECATION)
elseif(APPLE AND NOT APPLE_USE_ANGLE)
  # macOS: render through the system OpenGL framework (legacy 2.1 context).
  # See src/main/sdl2/gl_platform.hpp.
  find_package(OpenGL REQUIRED)
  target_link_libraries(cannonball-dx PRIVATE OpenGL::GL)
  target_compile_definitions(cannonball-dx PRIVATE GL_SILENCE_DEPRECATION)
elseif(APPLE)
  # macOS with ANGLE: SDL loads libEGL/libGLESv2 at runtime; only headers are needed.
  target_compile_definitions(cannonball-dx PRIVATE CANNONBALL_USE_GLES)
elseif(NOT WIN32)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(EGL    REQUIRED IMPORTED_TARGET egl)
  pkg_check_modules(GLESv2 REQUIRED IMPORTED_TARGET glesv2)
  # If you link with pkg-config libs, do it here:
  target_link_libraries(cannonball-dx PRIVATE
    PkgConfig::EGL
    PkgConfig::GLESv2
  )
else()
  # Windows: prefer ANGLE from vcpkg
  find_package(unofficial-angle CONFIG QUIET)
  if(TARGET unofficial::angle::libEGL AND TARGET unofficial::angle::libGLESv2)
    target_link_libraries(cannonball-dx PRIVATE
      unofficial::angle::libEGL
      unofficial::angle::libGLESv2
    )
  endif()
endif()


# Links
if(_SDL2_TARGETS)
    target_link_libraries(cannonball-dx PRIVATE ${_SDL2_TARGETS})
elseif(_SDL2_LINK_TARGET)
    target_link_libraries(cannonball-dx PRIVATE ${_SDL2_LINK_TARGET})
endif()

target_link_libraries(cannonball-dx PRIVATE
  ${_TINYXML2_TARGET}
  $<$<PLATFORM_ID:Windows>:unofficial::angle::libEGL>
  $<$<PLATFORM_ID:Windows>:unofficial::angle::libGLESv2>
  $<$<NOT:$<PLATFORM_ID:Windows,Darwin,iOS,visionOS>>:PkgConfig::EGL>
  $<$<NOT:$<PLATFORM_ID:Windows,Darwin,iOS,visionOS>>:PkgConfig::GLESv2>
)

# --- Linux: udev for joystick hotplugging ---
if(UNIX AND NOT APPLE)
    find_library(UDEV_LIBRARY udev)
    if(UDEV_LIBRARY)
        target_link_libraries(cannonball-dx PRIVATE ${UDEV_LIBRARY})
    else()
        message(WARNING "libudev not found. Joystick hotplugging may not work.")
    endif()
    # --- Also add debug trace info
    target_link_options(cannonball-dx PRIVATE -rdynamic)
endif()

# --- Optional MPG123/MP3 support ---
if(WITH_MP3)
    find_path(MPG123_INCLUDE_DIR mpg123.h)
    find_library(MPG123_LIBRARY NAMES mpg123 libmpg123)
    if(MPG123_INCLUDE_DIR AND MPG123_LIBRARY)
        message(STATUS "mpg123 found, enabling MP3 playback.")
        target_compile_definitions(cannonball-dx PRIVATE WITH_MP3)
        target_include_directories(cannonball-dx PRIVATE ${MPG123_INCLUDE_DIR})
        target_link_libraries(cannonball-dx PRIVATE ${MPG123_LIBRARY})
    else()
        message(WARNING "mpg123 not found, building without MP3 playback. Set WITH_MP3=OFF to suppress this warning.")
    endif()
else()
    message(STATUS "Building without MP3 playback support (WITH_MP3=OFF).")
endif()

# Windows-specific: DirectX and Windows multimedia (if needed)
if(WIN32)
    target_link_libraries(cannonball-dx PRIVATE dinput8 dxguid winmm)
endif()

# Recommended warnings for GCC/Clang
if(MSVC)
    target_compile_options(cannonball-dx PRIVATE /W4)
else()
    # C-family flags only: the visionOS target also compiles Swift.
    set(CB_C_LANGS "C,CXX,OBJC,OBJCXX")
    target_compile_options(cannonball-dx PRIVATE "$<$<COMPILE_LANGUAGE:${CB_C_LANGS}>:-Wall;-Wextra;-Wpedantic>")

    # Optimize Release builds aggressively (-Ofast) on GCC/Clang
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(cannonball-dx PRIVATE $<$<CONFIG:Release>:-Ofast>)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Clang deprecated -Ofast; this is its documented equivalent.
        target_compile_options(cannonball-dx PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:${CB_C_LANGS}>>:-O3;-ffast-math>")
    endif()
endif()

# Debug builds
# --- Debug hardening & sanitizers (GCC/Clang only) ------------------------------
# No explicit -msse4.1 here: the SIMD code detects SSE4.1 at compile time via
# __SSE4_1__ and falls back to an SSE2 path automatically.  Release builds with
# WITH_MARCH_NATIVE=ON enable SSE4.1 on capable hosts through -march=native.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  # Compile-time flags (Debug only)
  target_compile_options(cannonball-dx PRIVATE
    "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>>:-g;-O0;-fsanitize=address,undefined;-fno-omit-frame-pointer;-ftrapv>"
  )

  # Link-time flags (Debug only) — needed for sanitizers
  target_link_options(cannonball-dx PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
  )

  # Preprocessor defines (Debug only)
  target_compile_definitions(cannonball-dx PRIVATE
    $<$<CONFIG:Debug>:_FORTIFY_SOURCE=2>
  )
endif()


# --- Quiet warnings on noisy toolchains ---------------------------------------
# These are conservative suppressions aimed at legacy/portability code paths.
if(QUIET_BUILD)
    if(MSVC)
        # Drop to /W3 and silence a few common noise warnings (narrowing, signed/unsigned, deprecations)
        target_compile_options(cannonball-dx PRIVATE /W3 /wd4244 /wd4267 /wd4018 /wd4996)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(cannonball-dx PRIVATE
            "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-Wno-unused-parameter;-Wno-unused-variable;-Wno-missing-field-initializers;-Wno-sign-compare;-Wno-reorder;-Wno-implicit-fallthrough;-Wno-format-security>")
        if(SUPER_QUIET)
            # Nuke all warnings for user-friendly Release-like configs
            target_compile_options(cannonball-dx PRIVATE
            "$<$<AND:$<CONFIG:Release,RelWithDebInfo,MinSizeRel>,$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>>:-w>")
        endif()
    endif()
endif()


# Add -march=native for GCC/Clang, only in Release builds
if(WITH_MARCH_NATIVE AND NOT CMAKE_CROSSCOMPILING AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
    target_compile_options(cannonball-dx PRIVATE
      $<$<COMPILE_LANGUAGE:C>:-march=native>
      $<$<COMPILE_LANGUAGE:CXX>:-march=native>)
  endif()
endif()

option(WITH_RVV "Enable RISC-V Vector (RVV) SIMD (requires -march=..._v)" OFF)
if(WITH_RVV AND CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
    target_compile_options(cannonball-dx PRIVATE -march=rv64gcv)
endif()


# --- PROFILING FLAGS -----------------------------------------------------------
if(ENABLE_PROFILING)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # Symbols + frame pointers for accurate stacks in perf/flamegraphs
        target_compile_options(cannonball-dx PRIVATE -g -fno-omit-frame-pointer)
        # Avoid LTO which can hide symbols / confuse profilers
        set_property(TARGET cannonball-dx PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
    endif()

    if(PROFILE_WITH_PERF)
        message(STATUS "Profiling: perf enabled (symbols + frame pointers)")
        # Nothing extra required beyond -g and -fno-omit-frame-pointer
    endif()

    if(PROFILE_WITH_GPROF)
        message(STATUS "Profiling: gprof via -pg enabled")
        target_compile_options(cannonball-dx PRIVATE -pg)
        target_link_options(cannonball-dx PRIVATE -pg)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            include(CheckPIESupported)
            check_pie_supported(LANGUAGES C CXX)
            if(NOT CMAKE_CXX_LINK_PIE_SUPPORTED)
                target_link_options(cannonball-dx PRIVATE -no-pie)
            endif()
        endif()
    endif()

    if(PROFILE_WITH_CALLGRIND)
        message(STATUS "Profiling: callgrind-friendly inlining reduction enabled")
        target_compile_options(cannonball-dx PRIVATE -fno-inline-functions -fno-inline-small-functions)
    endif()

    if(PROFILE_WITH_GPERFTOOLS)
        # To enable this, run:
        # $ cmake -DENABLE_PROFILING=ON -DPROFILE_WITH_GPERFTOOLS=ON -S ../ -B build
        # $ cmake --build build -j
        #
        message(STATUS "Profiling: Google perftools (CPU) enabled")
        # Prefer pkg-config if available
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(LIBPROFILER QUIET libprofiler)
        endif()

        # Include dir is optional (header usually in system include path),
        # but probe to give nicer errors if not installed.
        find_path(GPERFTOOLS_INCLUDE_DIR
                  NAMES gperftools/profiler.h
                  HINTS ${LIBPROFILER_INCLUDE_DIRS})

        # Find the library (shared preferred)
        find_library(GPERFTOOLS_PROFILER_LIB
                     NAMES profiler libprofiler
                     HINTS ${LIBPROFILER_LIBRARY_DIRS})

        if(NOT GPERFTOOLS_INCLUDE_DIR OR NOT GPERFTOOLS_PROFILER_LIB)
            message(FATAL_ERROR "gperftools not found. Install libgoogle-perftools-dev (Ubuntu/WSL) then reconfigure.")
        endif()

        target_include_directories(cannonball-dx PRIVATE ${GPERFTOOLS_INCLUDE_DIR})
        target_link_libraries(cannonball-dx PRIVATE ${GPERFTOOLS_PROFILER_LIB} pthread)

        # Helpful define (optional): lets you #ifdef around ProfilerStart/Stop
        target_compile_definitions(cannonball-dx PRIVATE HAS_GPERFTOOLS=1)

        # Make -DPROFILE_WITH_GPERFTOOLS available to your code
        target_compile_definitions(cannonball-dx PRIVATE PROFILE_WITH_GPERFTOOLS=1)
        # Notes:
        # - Do NOT combine with -pg (gprof).
        # - You start/stop profiling in code with ProfilerStart/Stop.
        # - At runtime you can tune sampling with CPUPROFILE_FREQUENCY, e.g.:
        #     CPUPROFILE_FREQUENCY=1000 ./cannonball-dx
    endif()

endif()

# OpenMP: Try to enable, but don't require everywhere
# Apple Clang ships without an OpenMP runtime; pick up Homebrew's libomp if present.
if(APPLE AND NOT CB_APPLE_MOBILE AND NOT OpenMP_ROOT)
  foreach(_omp_prefix /opt/homebrew/opt/libomp /usr/local/opt/libomp)
    if(EXISTS "${_omp_prefix}/include/omp.h")
      set(OpenMP_ROOT "${_omp_prefix}")
      break()
    endif()
  endforeach()
endif()
# iOS/visionOS have no OpenMP runtime; omp_compat.hpp provides serial fallbacks.
if(NOT CB_APPLE_MOBILE)
    find_package(OpenMP)
endif()
if(OpenMP_CXX_FOUND)
    target_link_libraries(cannonball-dx PRIVATE OpenMP::OpenMP_CXX)
endif()

# Copy out DLLs on Windows
if(WIN32)
    add_custom_command(TARGET cannonball-dx POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:cannonball-dx> $<TARGET_FILE_DIR:cannonball-dx>
        COMMAND_EXPAND_LISTS
    )
endif()

# --- Apple branding overrides --------------------------------------------------
# Defaults below can be overridden by an untracked apple-overrides.cmake in the
# source root (git-ignored; see apple-overrides.cmake.example), or by pointing
# APPLE_OVERRIDES_FILE elsewhere. Used by the macOS app and future iOS/visionOS
# targets.
if(APPLE)
    set(APPLE_APP_NAME  "CannonBall DX")
    set(APPLE_APP_ICON  "${CMAKE_SOURCE_DIR}/res/icon.png")
    set(APPLE_BUNDLE_ID "com.cannonball-dx.CannonBallDX")
    set(APPLE_DEVELOPMENT_TEAM "")   # iOS device builds: your Apple team ID

    set(APPLE_OVERRIDES_FILE "${CMAKE_SOURCE_DIR}/apple-overrides.cmake"
        CACHE FILEPATH "Optional CMake file overriding APPLE_APP_NAME/APPLE_APP_ICON/APPLE_BUNDLE_ID/APPLE_DEVELOPMENT_TEAM")
    # Reconfigure when the overrides file is created, edited or removed.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${APPLE_OVERRIDES_FILE}")
    if(EXISTS "${APPLE_OVERRIDES_FILE}")
        include("${APPLE_OVERRIDES_FILE}")
        message(STATUS "Apple overrides: ${APPLE_OVERRIDES_FILE}")
    endif()

    # Relative icon paths are relative to the source root.
    get_filename_component(APPLE_APP_ICON "${APPLE_APP_ICON}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    if(NOT EXISTS "${APPLE_APP_ICON}")
        message(FATAL_ERROR "APPLE_APP_ICON not found: ${APPLE_APP_ICON}")
    endif()
    message(STATUS "Apple app: \"${APPLE_APP_NAME}\" (${APPLE_BUNDLE_ID}), icon ${APPLE_APP_ICON}")

    # Window title and SDL app name follow the app name.
    target_compile_definitions(cannonball-dx PRIVATE "CB_APP_NAME=\"${APPLE_APP_NAME}\"")
endif()

# --- macOS .app bundle ----------------------------------------------------------
# Produces "<APPLE_APP_NAME>.app". At launch the app runs from
# ~/Library/Application Support/CannonBall DX (config, saves, roms/), with res/
# linked back into the bundle. See macos_prepare_working_directory() in main.cpp.
# The data folder name is fixed so renaming the app keeps existing ROMs/config.
option(MACOS_APP_BUNDLE "Build a macOS .app bundle" ON)
if(APPLE AND NOT CB_APPLE_MOBILE AND MACOS_APP_BUNDLE)
    file(READ "${main_cpp_base}/globals.hpp" _cb_globals_hpp_mac)
    string(REGEX MATCH "CANNONBALL_DX_VERSION[ \t]+\"([^\"]+)\"" _cb_unused "${_cb_globals_hpp_mac}")
    set(CB_MAC_VERSION "${CMAKE_MATCH_1}")

    set_target_properties(cannonball-dx PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "${APPLE_APP_NAME}"
        MACOSX_BUNDLE_BUNDLE_NAME "${APPLE_APP_NAME}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${APPLE_BUNDLE_ID}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${CB_MAC_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${CB_MAC_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "AppIcon.icns")

    # App icon: every standard size, scaled from APPLE_APP_ICON (1024x1024 recommended).
    set(_cb_iconset "${CMAKE_CURRENT_BINARY_DIR}/AppIcon.iconset")
    set(_cb_icns "${CMAKE_CURRENT_BINARY_DIR}/AppIcon.icns")
    set(_cb_icon_cmds)
    foreach(_sz 16 32 128 256 512)
        math(EXPR _sz2 "${_sz} * 2")
        list(APPEND _cb_icon_cmds
            COMMAND sips -s format png -z ${_sz} ${_sz} "${APPLE_APP_ICON}" --out "${_cb_iconset}/icon_${_sz}x${_sz}.png"
            COMMAND sips -s format png -z ${_sz2} ${_sz2} "${APPLE_APP_ICON}" --out "${_cb_iconset}/icon_${_sz}x${_sz}@2x.png")
    endforeach()
    add_custom_command(OUTPUT "${_cb_icns}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${_cb_iconset}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_cb_iconset}"
        ${_cb_icon_cmds}
        COMMAND iconutil -c icns "${_cb_iconset}" -o "${_cb_icns}"
        DEPENDS "${APPLE_APP_ICON}"
        COMMENT "Generating AppIcon.icns from ${APPLE_APP_ICON}"
        VERBATIM)
    target_sources(cannonball-dx PRIVATE "${_cb_icns}")
    set_source_files_properties("${_cb_icns}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    # Copy res/ and the ROM readme into Contents/Resources on every build.
    add_custom_command(TARGET cannonball-dx POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/res" "$<TARGET_BUNDLE_CONTENT_DIR:cannonball-dx>/Resources/res"
        COMMAND ${CMAKE_COMMAND} -E copy
                "${CMAKE_SOURCE_DIR}/roms/roms.txt" "$<TARGET_BUNDLE_CONTENT_DIR:cannonball-dx>/Resources/roms.txt"
        VERBATIM)

    # Copy Homebrew/third-party dylibs into Contents/Frameworks and point the
    # executable at them, so the app runs on Macs without Homebrew.
    option(MACOS_BUNDLE_DYLIBS "Copy non-system dylibs into the .app" ON)
    set(MACOS_CODESIGN_IDENTITY "-" CACHE STRING "codesign identity for the bundle ('-' = ad-hoc)")
    if(MACOS_BUNDLE_DYLIBS)
        add_custom_command(TARGET cannonball-dx POST_BUILD
            COMMAND "${CMAKE_SOURCE_DIR}/tools/macos/bundle-dylibs.sh"
                    "$<TARGET_BUNDLE_DIR:cannonball-dx>" "${MACOS_CODESIGN_IDENTITY}"
            VERBATIM)
    endif()
endif()

# --- iOS / visionOS app -----------------------------------------------------------
# Configure with -G Xcode -DCMAKE_SYSTEM_NAME=iOS. The app keeps config and saves
# in Library/Application Support and reads ROMs from Documents, which the Files
# app shows (see apple_prepare_working_directory() in main.cpp).
if(CB_APPLE_MOBILE)
    file(READ "${main_cpp_base}/globals.hpp" _cb_globals_hpp_ios)
    string(REGEX MATCH "CANNONBALL_DX_VERSION[ \t]+\"([^\"]+)\"" _cb_unused "${_cb_globals_hpp_ios}")
    set(CB_IOS_VERSION "${CMAKE_MATCH_1}")
    # CFBundleVersion; CI passes its run number so every build is distinct.
    set(CB_BUILD_NUMBER "${CB_IOS_VERSION}" CACHE STRING "Bundle version (build number)")
    set(CB_IOS_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum iOS version")
    set(CB_VISIONOS_DEPLOYMENT_TARGET "2.0" CACHE STRING "Minimum visionOS version")

    # App icon from APPLE_APP_ICON (1024x1024 recommended).
    set(_cb_xcassets "${CMAKE_CURRENT_BINARY_DIR}/apple/Assets.xcassets")
    file(REMOVE_RECURSE "${_cb_xcassets}")
    set(_cb_json_info "\"info\" : { \"author\" : \"xcode\", \"version\" : 1 }")
    file(WRITE "${_cb_xcassets}/Contents.json" "{ ${_cb_json_info} }\n")
    if(CB_VISIONOS)
        # visionOS icons are layered image stacks and need at least two layers
        # with images: the icon is the back layer, the front layer is
        # transparent, and the middle layer is empty.
        set(_cb_icon_png "${_cb_xcassets}/AppIcon.solidimagestack/Back.solidimagestacklayer/Content.imageset/AppIcon-1024.png")
        file(WRITE "${_cb_xcassets}/AppIcon.solidimagestack/Contents.json"
            "{ ${_cb_json_info}, \"layers\" : [ { \"filename\" : \"Front.solidimagestacklayer\" }, { \"filename\" : \"Middle.solidimagestacklayer\" }, { \"filename\" : \"Back.solidimagestacklayer\" } ] }\n")
        foreach(_cb_layer Front Middle Back)
            set(_cb_layer_dir "${_cb_xcassets}/AppIcon.solidimagestack/${_cb_layer}.solidimagestacklayer")
            file(WRITE "${_cb_layer_dir}/Contents.json" "{ ${_cb_json_info} }\n")
            if(_cb_layer STREQUAL "Back")
                set(_cb_image "\"filename\" : \"AppIcon-1024.png\", ")
            elseif(_cb_layer STREQUAL "Front")
                file(COPY "${CMAKE_SOURCE_DIR}/platform/visionos/icon-transparent-layer.png"
                     DESTINATION "${_cb_layer_dir}/Content.imageset")
                set(_cb_image "\"filename\" : \"icon-transparent-layer.png\", ")
            else()
                set(_cb_image "")
            endif()
            file(WRITE "${_cb_layer_dir}/Content.imageset/Contents.json"
                "{ \"images\" : [ { ${_cb_image}\"idiom\" : \"vision\", \"scale\" : \"2x\" } ], ${_cb_json_info} }\n")
        endforeach()
    else()
        # A single 1024x1024 image; Xcode derives the other sizes.
        set(_cb_icon_png "${_cb_xcassets}/AppIcon.appiconset/AppIcon-1024.png")
        file(WRITE "${_cb_xcassets}/AppIcon.appiconset/Contents.json"
            "{ \"images\" : [ { \"filename\" : \"AppIcon-1024.png\", \"idiom\" : \"universal\", \"platform\" : \"ios\", \"size\" : \"1024x1024\" } ], ${_cb_json_info} }\n")
    endif()
    execute_process(
        COMMAND sips -s format png -z 1024 1024 "${APPLE_APP_ICON}" --out "${_cb_icon_png}"
        OUTPUT_QUIET RESULT_VARIABLE _cb_sips_rc)
    if(_cb_sips_rc EQUAL 0 AND CB_VISIONOS)
        # The back layer of a visionOS icon must be fully opaque (the default icon is a
        # circle with transparent corners). A round trip through JPEG drops the alpha;
        # the corners turn white, outside the circle visionOS crops the icon to.
        set(_cb_icon_jpg "${CMAKE_CURRENT_BINARY_DIR}/apple/AppIcon-opaque.jpg")
        execute_process(
            COMMAND sips -s format jpeg -s formatOptions best "${_cb_icon_png}" --out "${_cb_icon_jpg}"
            OUTPUT_QUIET RESULT_VARIABLE _cb_sips_rc)
        if(_cb_sips_rc EQUAL 0)
            execute_process(
                COMMAND sips -s format png "${_cb_icon_jpg}" --out "${_cb_icon_png}"
                OUTPUT_QUIET RESULT_VARIABLE _cb_sips_rc)
        endif()
    endif()
    if(NOT _cb_sips_rc EQUAL 0)
        message(FATAL_ERROR "Could not convert ${APPLE_APP_ICON} to a 1024x1024 PNG")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${APPLE_APP_ICON}")
    # Game resources go in the bundle root (iOS bundles are flat). Listing them
    # as resources lets Xcode's Copy Bundle Resources phase copy and sign them;
    # res/ is added as a folder reference so its layout is kept.
    enable_language(OBJCXX)
    if(CB_VISIONOS)
        # main() for sdl2-compat, the window-shape helper, and the embedded
        # SDL3 framework that sdl2-compat loads at runtime.
        enable_language(Swift)
        set(_cb_platform_sources
            "${CMAKE_SOURCE_DIR}/platform/visionos/uikit_main.c"
            "${CMAKE_SOURCE_DIR}/platform/visionos/visionos_window.mm"
            # Stereoscopic depth layers (3D DEPTH setting).
            "${CMAKE_SOURCE_DIR}/platform/visionos/StereoOverlay.swift")
        set(_cb_info_plist "${CMAKE_SOURCE_DIR}/platform/visionos/Info.plist.in")
        add_dependencies(cannonball-dx SDL3-shared)
        set_target_properties(cannonball-dx PROPERTIES
            XCODE_EMBED_FRAMEWORKS SDL3-shared
            XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY ON
            XCODE_EMBED_FRAMEWORKS_REMOVE_HEADERS_ON_COPY ON
            XCODE_ATTRIBUTE_XROS_DEPLOYMENT_TARGET "${CB_VISIONOS_DEPLOYMENT_TARGET}"
            XCODE_ATTRIBUTE_SWIFT_VERSION "5.0"
            # CMake bakes in the Swift library path of the configure-time SDK
            # (the device SDK); resolve it per build so simulator builds link.
            XCODE_ATTRIBUTE_LIBRARY_SEARCH_PATHS "$(SDKROOT)/usr/lib/swift $(inherited)"
            XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "7")
    else()
        # UIScene lifecycle adoption for SDL2 (required on iOS 27+).
        set(_cb_platform_sources "${CMAKE_SOURCE_DIR}/platform/ios/ios_scene.mm")
        set(_cb_info_plist "${CMAKE_SOURCE_DIR}/platform/ios/Info.plist.in")
        set_target_properties(cannonball-dx PROPERTIES
            XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "${CB_IOS_DEPLOYMENT_TARGET}"
            XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")
    endif()
    target_sources(cannonball-dx PRIVATE ${_cb_platform_sources})
    foreach(_cb_src ${_cb_platform_sources})
        if(_cb_src MATCHES "\\.mm$")
            set_source_files_properties("${_cb_src}" PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
        endif()
    endforeach()
    target_link_libraries(cannonball-dx PRIVATE "-framework UIKit")

    set(_cb_ios_resources
        "${_cb_xcassets}"
        "${CMAKE_SOURCE_DIR}/res"
        "${CMAKE_SOURCE_DIR}/roms/roms.txt")
    target_sources(cannonball-dx PRIVATE ${_cb_ios_resources})

    set_target_properties(cannonball-dx PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "${APPLE_APP_NAME}"
        RESOURCE "${_cb_ios_resources}"
        MACOSX_BUNDLE_INFO_PLIST "${_cb_info_plist}"
        MACOSX_BUNDLE_BUNDLE_NAME "${APPLE_APP_NAME}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${APPLE_BUNDLE_ID}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${CB_BUILD_NUMBER}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${CB_IOS_VERSION}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${APPLE_BUNDLE_ID}"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${APPLE_DEVELOPMENT_TEAM}"
        # The app is what an archive installs (see SKIP_INSTALL above).
        XCODE_ATTRIBUTE_SKIP_INSTALL NO
        XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)")
endif()

# Copy out config files
configure_file(res/tilemap.bin ${CMAKE_CURRENT_BINARY_DIR}/tilemap.bin COPYONLY)
configure_file(res/tilepatch.bin ${CMAKE_CURRENT_BINARY_DIR}/tilepatch.bin COPYONLY)

# -----------------------------
# Install: man page & licenses
# -----------------------------

# Install the cannonball-dx(6) man page if present
set(_MAN_CANDIDATES
  "${CMAKE_SOURCE_DIR}/docs/cannonball-dx.6"
  "${CMAKE_SOURCE_DIR}/cannonball-dx.6")
foreach(_cand IN LISTS _MAN_CANDIDATES)
  if(EXISTS "${_cand}")
    set(MANPAGE_SOURCE "${_cand}")
    break()
  endif()
endforeach()
if(INSTALL_MANPAGE AND UNIX AND MANPAGE_SOURCE)
  get_filename_component(_ext "${MANPAGE_SOURCE}" EXT)
  install(FILES "${MANPAGE_SOURCE}" DESTINATION "${CMAKE_INSTALL_MANDIR}/man6")
endif()

# Install upstream license and third-party notices
install(FILES "${CMAKE_SOURCE_DIR}/license.txt"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}")
if(EXISTS "${CMAKE_SOURCE_DIR}/THIRD-PARTY-NOTICES.md")
  install(FILES "${CMAKE_SOURCE_DIR}/THIRD-PARTY-NOTICES.md"
          DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/licenses")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/licenses")
  install(DIRECTORY "${CMAKE_SOURCE_DIR}/licenses/"
          DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/licenses")
endif()
