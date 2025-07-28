# CMakeLists.txt
#
# Copyright (C) 2025, Charles Chiou

cmake_minimum_required(VERSION 3.13)

include (pico-sdk/external/pico_sdk_import.cmake)
pico_sdk_init()

project(meshmon VERSION 1.0.0 LANGUAGES C CXX ASM)

add_subdirectory(libmeshtastic)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_CURRENT_LIST_DIR})
add_compile_options(-Wall -Wextra -Werror)
add_compile_options(-g -O2)

set(PICO_STACK_SIZE 4096)

set(WHOAMI_OUTPUT)
execute_process(
  COMMAND whoami
  OUTPUT_VARIABLE WHOAMI_OUTPUT
  RESULT_VARIABLE WHOAMI_RESULT
  ERROR_VARIABLE WHOAMI_ERROR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )
set(HOSTNAME_OUTPUT)
execute_process(
  COMMAND hostname
  OUTPUT_VARIABLE HOSTNAME_OUTPUT
  RESULT_VARIABLE HOSTNAME_RESULT
  ERROR_VARIABLE HOSTNAME_ERROR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )
set(DATE_OUTPUT)
execute_process(
  COMMAND date "+%Y-%m-%d %H:%M:%S"
  OUTPUT_VARIABLE DATE_OUTPUT
  RESULT_VARIABLE DATE_RESULT
  ERROR_VARIABLE DATE_ERROR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/version.h
  @ONLY
  )
add_executable(meshroom
  led.c
  serial.c
  shell.c
  MeshRoom.cxx
  meshroom.cxx)
target_include_directories(meshroom PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(meshroom pico_stdlib libmeshtastic)
pico_enable_stdio_uart(meshroom 0)
pico_add_extra_outputs(meshroom)
