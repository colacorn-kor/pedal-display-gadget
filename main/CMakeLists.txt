set(APP_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/../app_main.c"
    "${CMAKE_CURRENT_LIST_DIR}/../app_images.c"
    "${CMAKE_CURRENT_LIST_DIR}/../app_monitor.c"
    "${CMAKE_CURRENT_LIST_DIR}/../app_slots.c"
    "${CMAKE_CURRENT_LIST_DIR}/../app_tuner.c"
    "${CMAKE_CURRENT_LIST_DIR}/../content_screen.c"
    "${CMAKE_CURRENT_LIST_DIR}/../display_bringup.c"
    "${CMAKE_CURRENT_LIST_DIR}/../fft_map.c"
    "${CMAKE_CURRENT_LIST_DIR}/../gadget_app.c"
    "${CMAKE_CURRENT_LIST_DIR}/../midi.c"
    "${CMAKE_CURRENT_LIST_DIR}/../midi_map.c"
    "${CMAKE_CURRENT_LIST_DIR}/../renderer.c"
    "${CMAKE_CURRENT_LIST_DIR}/../renderer_bars.c"
    "${CMAKE_CURRENT_LIST_DIR}/../renderer_curve.c"
    "${CMAKE_CURRENT_LIST_DIR}/../renderer_reactive.c"
    "${CMAKE_CURRENT_LIST_DIR}/../screen_manager.c"
    "${CMAKE_CURRENT_LIST_DIR}/../theme.c"
    "${CMAKE_CURRENT_LIST_DIR}/../tuner.c"
    "${CMAKE_CURRENT_LIST_DIR}/../tuner_screen.c"
)

idf_component_register(SRCS ${APP_SOURCES} INCLUDE_DIRS "..")

