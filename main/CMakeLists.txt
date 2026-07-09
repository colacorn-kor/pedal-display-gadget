set(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
include("${PROJECT_ROOT}/sim/UI_SOURCES.cmake")

set(APP_SOURCES
    "${PROJECT_ROOT}/app_main.c"
    "${PROJECT_ROOT}/display_bringup.c"
    "${PROJECT_ROOT}/fft_map.c"
    "${PROJECT_ROOT}/midi.c"
    "${PROJECT_ROOT}/midi_map.c"
    "${PROJECT_ROOT}/music_events.c"
    "${PROJECT_ROOT}/platform_esp.c"
    "${PROJECT_ROOT}/tuner.c"
    ${PEDAL_UI_SOURCES}
)

idf_component_register(SRCS ${APP_SOURCES} INCLUDE_DIRS "..")

