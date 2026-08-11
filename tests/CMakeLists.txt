cmake_minimum_required(VERSION 3.16)
project(pedal_display_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

add_executable(test_midi test_midi.c ../midi.c)
target_include_directories(test_midi PRIVATE ..)
add_test(NAME midi_parser COMMAND test_midi)

add_executable(test_midi_service test_midi_service.c ../midi_service.c)
target_include_directories(test_midi_service PRIVATE ..)
add_test(NAME midi_service_snapshot COMMAND test_midi_service)

add_executable(test_tuner test_tuner.c ../tuner.c)
target_include_directories(test_tuner PRIVATE ..)
if(NOT MSVC)
    target_link_libraries(test_tuner PRIVATE m)
endif()
add_test(NAME tuner_detection COMMAND test_tuner)

add_executable(test_audio_level test_audio_level.c ../audio_level.c)
target_include_directories(test_audio_level PRIVATE ..)
if(NOT MSVC)
    target_link_libraries(test_audio_level PRIVATE m)
endif()
add_test(NAME audio_level_units COMMAND test_audio_level)

add_executable(test_image_probe
    test_image_probe.c
    ../image_probe.c
)
target_include_directories(test_image_probe PRIVATE ..)
add_test(NAME image_file_probe COMMAND test_image_probe)

add_executable(test_audio_playback
    test_audio_playback.c
    ../audio_playback.c
)
target_include_directories(test_audio_playback PRIVATE ..)
if(NOT MSVC)
    target_link_libraries(test_audio_playback PRIVATE m)
endif()
add_test(NAME audio_playback_transport_mixer COMMAND test_audio_playback)

add_executable(test_audio_effects
    test_audio_effects.c
    ../audio_effects.c
    ../audio_playback.c
)
target_include_directories(test_audio_effects PRIVATE ..)
if(NOT MSVC)
    target_link_libraries(test_audio_effects PRIVATE m)
endif()
add_test(NAME audio_effects_bus COMMAND test_audio_effects)

add_executable(test_metronome_engine
    test_metronome_engine.c
    ../metronome_engine.c
)
target_include_directories(test_metronome_engine PRIVATE ..)
if(NOT MSVC)
    target_link_libraries(test_metronome_engine PRIVATE m)
endif()
add_test(NAME metronome_sample_clock COMMAND test_metronome_engine)

add_executable(test_music_sources
    test_music_sources.c
    ../audio_playback.c
    ../music_lobby.c
    ../wav_decoder.c
)
target_include_directories(test_music_sources PRIVATE ..)
if(NOT MSVC)
    target_link_libraries(test_music_sources PRIVATE m)
endif()
add_test(NAME music_wav_and_lobby_sources COMMAND test_music_sources)

add_executable(test_spectrum_weighting
    test_spectrum_weighting.c
    ../spectrum_weighting.c
)
target_include_directories(test_spectrum_weighting PRIVATE ..)
if(NOT MSVC)
    target_link_libraries(test_spectrum_weighting PRIVATE m)
endif()
add_test(NAME spectrum_weighting_response COMMAND test_spectrum_weighting)

add_executable(test_audio_autorange
    test_audio_autorange.c
    ../audio_autorange.c
)
target_include_directories(test_audio_autorange PRIVATE ..)
if(NOT MSVC)
    target_link_libraries(test_audio_autorange PRIVATE m)
endif()
add_test(NAME audio_autorange_selection COMMAND test_audio_autorange)

add_executable(test_fft_map
    test_fft_map.c
    ../fft_map.c
    ../sim/sim_fft_backend.c
)
target_include_directories(test_fft_map PRIVATE ../sim/compat ..)
if(NOT MSVC)
    target_link_libraries(test_fft_map PRIVATE m)
endif()
add_test(NAME fft_map_shared_backend COMMAND test_fft_map)

add_executable(test_storage test_storage.c ../storage.c)
target_include_directories(test_storage PRIVATE ..)
add_test(NAME storage_media_catalog COMMAND test_storage)

add_executable(test_game_runtime
    test_game_runtime.c
    ../game_core_peanut.c
    ../game_runtime.c
)
target_include_directories(test_game_runtime PRIVATE ../sim/compat ..)
add_test(NAME game_boy_runtime COMMAND test_game_runtime)

add_executable(test_app_slots_migration
    test_app_slots_migration.c
    ../app_slots.c
)
target_include_directories(test_app_slots_migration PRIVATE
    compat
    ../sim/compat
    ..
)
add_test(NAME app_slots_v5_migration COMMAND test_app_slots_migration)

if(MSVC)
    set_source_files_properties(
        ../game_core_peanut.c
        PROPERTIES COMPILE_OPTIONS "/wd4244;/wd4324"
    )
    target_compile_options(test_midi PRIVATE /utf-8 /wd4819)
    target_compile_options(test_midi_service PRIVATE
        /utf-8
        /wd4819
        /experimental:c11atomics
    )
    target_compile_options(test_tuner PRIVATE
        /utf-8
        /wd4819
        /experimental:c11atomics
    )
    target_compile_options(test_audio_level PRIVATE /utf-8 /wd4819)
    target_compile_options(test_image_probe PRIVATE /utf-8 /wd4819)
    target_compile_options(test_audio_playback PRIVATE
        /utf-8
        /wd4819
        /experimental:c11atomics
    )
    target_compile_options(test_audio_effects PRIVATE
        /utf-8
        /wd4819
        /experimental:c11atomics
    )
    target_compile_options(test_metronome_engine PRIVATE /utf-8 /wd4819)
    target_compile_options(test_music_sources PRIVATE
        /utf-8
        /wd4819
        /experimental:c11atomics
    )
    target_compile_options(test_spectrum_weighting PRIVATE /utf-8 /wd4819)
    target_compile_options(test_audio_autorange PRIVATE /utf-8 /wd4819)
    target_compile_options(test_fft_map PRIVATE /utf-8 /wd4819)
    target_compile_options(test_storage PRIVATE /utf-8 /wd4819)
    target_compile_options(test_game_runtime PRIVATE /utf-8 /wd4819)
    target_compile_options(test_app_slots_migration PRIVATE
        /utf-8
        /wd4819
        /experimental:c11atomics
    )
endif()

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME fft_normalization_reference
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_fft_normalization.py)
endif()
