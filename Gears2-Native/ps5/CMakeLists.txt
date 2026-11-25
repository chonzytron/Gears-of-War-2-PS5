# Gears of War 2 for PS5: the recompiled game (generated/tu6, made by the SDK's
# code generator from the player's own default.xex + Title Update 6 with
# COGBox's manifest), COGBox's game layer, and the ReXGlue runtime and its
# Xenos GPU backend, built as one static PS5 homebrew program. Build steps:
# ps5/BUILD.md. (After the Dead Space 2 PS5 port's ps5/CMakeLists.txt.)
#
# COGBox's PC build links an installed SDK with a runtime DLL. A PS5 app has no
# shared libraries of its own, so the SDK source (the pinned nightly +
# COGBox's patches + ps5/sdk/rexglue_ps5.patch, put in place by
# ps5/tools/fetch_sdk.sh) is built here, in this tree.
#
# GOW2_PS5_LINUX=ON builds the same program for Linux instead, to test it on a
# PC before it goes on a console: the same app and SDK, the runtime and GPU
# backend linked in as on the PS5, the PS5's diagnostics and profiler; SDL's
# window, pad and sound where the PS5 has its own.
cmake_minimum_required(VERSION 3.25)
project(gow2-ps5 LANGUAGES C CXX)

option(GOW2_PS5_LINUX "Build the PS5 program for Linux, to test it on a PC" OFF)
if(NOT PS5 AND NOT GOW2_PS5_LINUX)
    message(FATAL_ERROR "Configure with -DCMAKE_TOOLCHAIN_FILE=ps5/cmake/ps5-toolchain.cmake (ps5/BUILD.md), "
                        "or with -DGOW2_PS5_LINUX=ON for the Linux test build")
endif()
if(GOW2_PS5_LINUX AND PS5)
    message(FATAL_ERROR "GOW2_PS5_LINUX is the Linux build: configure it without the PS5 toolchain")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS OFF)
# The PS5 SDK's libc++ 18: fmt's compile-time format checks are off, as in the GTA IV and DS2 PS5 ports.
add_compile_definitions(FMT_USE_CONSTEVAL=0)

option(GOW2_PS5_THINLTO "ThinLTO for the PS5 program" OFF)
if(GOW2_PS5_THINLTO)
    add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:-flto=thin>")
endif()

get_filename_component(GOW2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)   # Gears2-Native
set(GOW2_SRC "${GOW2_ROOT}/src")
set(GOW2_GEN "${GOW2_ROOT}/generated/tu6" CACHE PATH "The recompiled game (rexglue codegen gow2_manifest_tu6.toml)")
set(GOW2_PS5_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
file(STRINGS "${GOW2_PS5_DIR}/VERSION.txt" GOW2_PS5_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${GOW2_PS5_DIR}/VERSION.txt")
message(STATUS "Gears of War 2 for PS5 ${GOW2_PS5_VERSION}")
if(NOT EXISTS "${GOW2_GEN}/sources.cmake")
    message(FATAL_ERROR "No recompiled game at ${GOW2_GEN}: run the code generator first (ps5/BUILD.md)")
endif()

# --- The SDK, built in tree -------------------------------------------------
set(REXSDK_DIR "${GOW2_ROOT}/third_party/rexglue-sdk-ps5" CACHE PATH "Patched ReXGlue SDK source (ps5/tools/fetch_sdk.sh)")
if(NOT EXISTS "${REXSDK_DIR}/include/rex/ps5_compat.h")
    message(FATAL_ERROR "No PS5-patched ReXGlue SDK at ${REXSDK_DIR}: run ps5/tools/fetch_sdk.sh first")
endif()
set(REXGLUE_USE_VULKAN ON CACHE BOOL "" FORCE)
set(REXGLUE_ENABLE_TRACY OFF CACHE BOOL "" FORCE)
set(REXGLUE_ENABLE_PERF_COUNTERS OFF CACHE BOOL "" FORCE)
set(REXGLUE_ENABLE_FIDELITYFX OFF CACHE BOOL "" FORCE)
set(REXGLUE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(REXGLUE_PS5_LINUX_TEST ${GOW2_PS5_LINUX} CACHE BOOL "" FORCE)
add_subdirectory("${REXSDK_DIR}" rexglue-sdk)

# --- How the game code is compiled ------------------------------------------
# COGBox's PC recipe (Gears2-Native/CMakeLists.txt): CMake's Release level, no fused multiply-adds (the PowerPC code
# keeps them separate), no SDK perf counter on every indirect call. Its CPU level x86-64-v3 is inside the PS5's Zen 2
# (-march=znver2, set by the toolchain on the PS5 and below for Linux).
set(GOW2_OPT "-O3" CACHE STRING "Release optimisation level of our code and the recompiled code (COGBox: CMake's -O3)")
set(CMAKE_CXX_FLAGS_RELEASE "${GOW2_OPT} -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "${GOW2_OPT} -DNDEBUG")
set(GOW2_CODE_OPTIONS -ffp-contract=off)
if(GOW2_PS5_LINUX)
    list(APPEND GOW2_CODE_OPTIONS -march=znver2)
endif()

# --- The recompiled game ----------------------------------------------------
include("${GOW2_GEN}/sources.cmake")
list(LENGTH GENERATED_SOURCES _gow2_n)
message(STATUS "gow2: compiling the recompiled game (${_gow2_n} sources)")
add_library(gow2_recomp OBJECT ${GENERATED_SOURCES})
target_include_directories(gow2_recomp PRIVATE "${GOW2_GEN}" "${GOW2_PS5_DIR}/src")
target_link_libraries(gow2_recomp PRIVATE rex::runtime)
rexglue_apply_target_settings(gow2_recomp)
# (line tables: a crash address in the log maps back to a recompiled function)
target_compile_options(gow2_recomp PRIVATE ${GOW2_CODE_OPTIONS} -gline-tables-only -UREXGLUE_ENABLE_PROFILING)
# generated/tu6/gow2_pch.h with the PS5's guest memory offset after it.
target_precompile_headers(gow2_recomp PRIVATE "${GOW2_PS5_DIR}/src/gow2_ps5_pch.h")

# --- COGBox's game layer ----------------------------------------------------
# The sources the manifest's 36 mid-function hooks and the app's calls need (layer 1-3 and the parts of layer 0 they
# use), compiled as COGBox has them. COGBox's own renderer, in-game menu, crash report and Windows tools are not here
# (ps5/src/gow2_ps5_cogbox_glue.cpp answers for them).
set(GOW2_COGBOX_SOURCES
    layer0_xenos/achievement_popup_assets.cpp
    layer0_xenos/achievements_data.cpp
    layer0_xenos/achievements_export.cpp
    layer0_xenos/achievements_popup_logic.cpp
    layer0_xenos/cheat_popup.cpp
    layer0_xenos/display_gamma.cpp
    layer0_xenos/frame_log.cpp
    layer0_xenos/hold_guest.cpp
    layer0_xenos/observer_hub.cpp
    layer0_xenos/player_tags.cpp
    layer0_xenos/pm4.cpp
    layer0_xenos/pm4_shadow.cpp
    layer0_xenos/ui_lang.cpp
    layer1_xdk7645/d3d_watch.cpp
    layer1_xdk7645/resource_check.cpp
    layer2_ue3/config_bundle.cpp
    layer2_ue3/config_install.cpp
    layer2_ue3/launch_command_line.cpp
    layer2_ue3/console.cpp
    layer2_ue3/main_menu_exit.cpp
    layer2_ue3/name_tags.cpp
    layer2_ue3/objects.cpp
    layer2_ue3/package_exports.cpp
    layer2_ue3/prebuild_lzx.cpp
    layer2_ue3/shader_prebuild.cpp
    layer3_gow2/adaptive_triggers.cpp
    layer3_gow2/cheats.cpp
    layer3_gow2/first_person.cpp
    layer3_gow2/game_patches.cpp
    layer3_gow2/main_menu_exit.cpp
    layer3_gow2/mouse_look.cpp
    layer3_gow2/unlocks.cpp
    layer3_gow2/untiled.cpp
)
list(TRANSFORM GOW2_COGBOX_SOURCES PREPEND "${GOW2_SRC}/")
add_library(gow2_cogbox OBJECT ${GOW2_COGBOX_SOURCES})
target_include_directories(gow2_cogbox PUBLIC
    "${GOW2_SRC}/layer0_xenos/include" "${GOW2_SRC}/layer1_xdk7645/include" "${GOW2_SRC}/layer2_ue3/include"
    "${GOW2_SRC}/layer3_gow2" "${GOW2_SRC}")
target_link_libraries(gow2_cogbox PRIVATE rex::runtime)
rexglue_apply_target_settings(gow2_cogbox)
target_compile_definitions(gow2_cogbox PRIVATE GOW2_TU6=1)
target_compile_options(gow2_cogbox PRIVATE ${GOW2_CODE_OPTIONS} -Wno-deprecated-declarations)
set_source_files_properties("${GOW2_SRC}/layer2_ue3/console.cpp" PROPERTIES COMPILE_DEFINITIONS GN_UE3_CONSOLE_GEARS2=1)

# --- The app ----------------------------------------------------------------
add_executable(gow2
    "${GOW2_PS5_DIR}/src/main.cpp"
    "${GOW2_PS5_DIR}/src/gow2_ps5_install.cpp"
    "${GOW2_PS5_DIR}/src/gow2_ps5_script.cpp"
    "${GOW2_PS5_DIR}/src/gow2_ps5_clears.cpp"
    "${GOW2_PS5_DIR}/src/gow2_ps5_cogbox_glue.cpp"
)
if(GOW2_PS5_LINUX)
    # What the console's system libraries answer, for Linux; and the SDK's switch for its PS5 program parts
    # (rex/platform.h, REX_PS5_PROGRAM), which the SDK sets for its own files only.
    target_sources(gow2 PRIVATE "${GOW2_PS5_DIR}/src/gow2_ps5_linux.cpp")
    foreach(t gow2 gow2_recomp gow2_cogbox)
        target_compile_definitions(${t} PRIVATE REX_PS5_LINUX_TEST=1)
    endforeach()
endif()
target_include_directories(gow2 PRIVATE
    "${GOW2_GEN}" "${GOW2_PS5_DIR}/src"
    # crypto/sha256.h: checking the player's files against the ones the code was made from. The code itself is
    # already in the runtime (xboxkrnl_crypt.cpp).
    "${REXSDK_DIR}/thirdparty"
    "${REXSDK_DIR}/thirdparty/imgui"
)
target_compile_definitions(gow2 PRIVATE GOW2_TU6=1 GOW2_PS5_VERSION="${GOW2_PS5_VERSION}")
target_compile_options(gow2 PRIVATE ${GOW2_CODE_OPTIONS})
# Which program it is (gow2_build_stamp.h, logged at start).
add_custom_target(gow2_build_stamp
    COMMAND "${CMAKE_COMMAND}" -DGOW2_REPO=${GOW2_ROOT} -DGOW2_PS5_DIR=${GOW2_PS5_DIR}
            -DGOW2_STAMP=${CMAKE_CURRENT_BINARY_DIR}/stamp/gow2_build_stamp.h
            -P "${GOW2_PS5_DIR}/cmake/build_stamp.cmake"
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/stamp/gow2_build_stamp.h"
    VERBATIM)
add_dependencies(gow2 gow2_build_stamp)
target_include_directories(gow2 PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/stamp")
if(PS5)
    foreach(t gow2 gow2_recomp gow2_cogbox)
        target_compile_options(${t} PRIVATE "SHELL:-include ${REXSDK_DIR}/include/rex/ps5_compat.h")
    endforeach()
endif()
# The runtime and the Xenos backend, both static here (the backend is created directly by the SDK's
# gpu_plugin_loader.cpp on the PS5).
target_link_libraries(gow2 PRIVATE
    gow2_recomp
    gow2_cogbox
    rex::runtime
    rexgpu-xenos
)
if(PS5)
    target_link_libraries(gow2 PRIVATE SceUserService ScePad SceAudioOut SceSystemService SceVideoOut)
endif()
rexglue_apply_target_settings(gow2)
# The SDK's app template and entry point (rex_app.cpp, windowed_app_main_sdl.cpp).
rexglue_configure_target(gow2)
if(PS5)
    # CMake's own link is skipped: ps5/tools/ps5_package.sh links the program with the PS5 Vulkan driver and start-up
    # code and packages it.
    set_property(TARGET gow2 PROPERTY RULE_LAUNCH_LINK "${GOW2_PS5_DIR}/tools/ps5_skip_link.sh")
endif()
