cmake_minimum_required(VERSION 3.16)
project(pedal_display_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

add_executable(test_midi test_midi.c ../midi.c)
target_include_directories(test_midi PRIVATE ..)
add_test(NAME midi_parser COMMAND test_midi)

add_executable(test_tuner test_tuner.c ../tuner.c)
target_include_directories(test_tuner PRIVATE ..)
if(NOT MSVC)
    target_link_libraries(test_tuner PRIVATE m)
endif()
add_test(NAME tuner_detection COMMAND test_tuner)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME fft_normalization_reference
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_fft_normalization.py)
endif()
