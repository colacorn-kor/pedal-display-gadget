set(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
include("${PROJECT_ROOT}/sim/UI_SOURCES.cmake")

set(APP_SOURCES
    "${PROJECT_ROOT}/app_main.c"
    "${PROJECT_ROOT}/audio_autorange.c"
    "${PROJECT_ROOT}/display_bringup.c"
    "${PROJECT_ROOT}/fft_map.c"
    "${PROJECT_ROOT}/midi.c"
    "${PROJECT_ROOT}/midi_map.c"
    "${PROJECT_ROOT}/music_events.c"
    "${PROJECT_ROOT}/platform_esp.c"
    "${PROJECT_ROOT}/storage_esp.c"
    "${PROJECT_ROOT}/tuner.c"
    ${PEDAL_UI_SOURCES}
)

idf_component_register(SRCS ${APP_SOURCES} INCLUDE_DIRS "..")

if(DEFINED INPUT_TRS_LADDER)
    target_compile_definitions(
        ${COMPONENT_LIB} PRIVATE INPUT_TRS_LADDER=${INPUT_TRS_LADDER}
    )
endif()

if(DEFINED AUDIO_DUAL_RANGE)
    target_compile_definitions(
        ${COMPONENT_LIB} PRIVATE AUDIO_DUAL_RANGE=${AUDIO_DUAL_RANGE}
    )
endif()

foreach(calibration_var
        AUDIO_DUAL_HOT_VOLTAGE_CORRECTION
        AUDIO_DUAL_SENSITIVE_VOLTAGE_CORRECTION
        AUDIO_GG_INPUT_FULL_SCALE_VPEAK)
    if(DEFINED ${calibration_var})
        target_compile_definitions(
            ${COMPONENT_LIB} PRIVATE
            ${calibration_var}=${${calibration_var}}
        )
    endif()
endforeach()
