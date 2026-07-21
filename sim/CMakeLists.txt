cmake_minimum_required(VERSION 3.22)

project(pedal_display_sim LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

find_package(SDL2 CONFIG QUIET)
if(TARGET SDL2::SDL2)
    set(PEDAL_SDL_TARGET SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
    set(PEDAL_SDL_TARGET SDL2::SDL2-static)
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
    endif()
    if(TARGET PkgConfig::SDL2)
        set(PEDAL_SDL_TARGET PkgConfig::SDL2)
    else()
        find_path(PEDAL_SDL_INCLUDE_DIR SDL.h PATH_SUFFIXES SDL2)
        find_library(PEDAL_SDL_LIBRARY SDL2)
        if(NOT PEDAL_SDL_INCLUDE_DIR OR NOT PEDAL_SDL_LIBRARY)
            message(FATAL_ERROR "SDL2 not found. Install SDL2 or pass SDL2_DIR / CMAKE_PREFIX_PATH.")
        endif()
    endif()
endif()

include(FetchContent)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(lvgl SOURCE_DIR "${PROJECT_ROOT}/managed_components/lvgl__lvgl")
FetchContent_MakeAvailable(lvgl)

if(PEDAL_SDL_TARGET)
    target_link_libraries(lvgl PUBLIC ${PEDAL_SDL_TARGET})
else()
    target_include_directories(lvgl PUBLIC "${PEDAL_SDL_INCLUDE_DIR}")
    target_link_libraries(lvgl PUBLIC "${PEDAL_SDL_LIBRARY}")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/UI_SOURCES.cmake")

add_executable(pedal_sim
    "${CMAKE_CURRENT_LIST_DIR}/platform_sim.c"
    "${CMAKE_CURRENT_LIST_DIR}/sim_audio.c"
    "${CMAKE_CURRENT_LIST_DIR}/sim_main.c"
    "${PROJECT_ROOT}/music_events.c"
    "${PROJECT_ROOT}/tuner.c"
    ${PEDAL_UI_SOURCES}
)

target_include_directories(pedal_sim PRIVATE
    "${PROJECT_ROOT}"
    "${CMAKE_CURRENT_LIST_DIR}/compat"
)

target_compile_definitions(pedal_sim PRIVATE SDL_MAIN_HANDLED)

if(MSVC)
    target_compile_options(pedal_sim PRIVATE /W4)
    target_compile_options(pedal_sim PRIVATE "$<$<COMPILE_LANGUAGE:C>:/std:c11>")
    target_compile_options(pedal_sim PRIVATE "$<$<COMPILE_LANGUAGE:C>:/experimental:c11atomics>")
    target_compile_options(pedal_sim PRIVATE /utf-8 /wd4819)
    target_compile_options(lvgl PRIVATE /utf-8 /wd4819 /wd5287)
    target_compile_definitions(pedal_sim PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(pedal_sim PRIVATE -Wall -Wextra)
    if(NOT WIN32)
        target_link_libraries(pedal_sim PRIVATE m)
    endif()
endif()

target_link_libraries(pedal_sim PRIVATE lvgl::lvgl)
