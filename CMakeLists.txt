# CMakeLists.txt
#
# Copyright (C) 2025, Charles Chiou

cmake_minimum_required(VERSION 3.13)

include (pico-sdk/external/pico_sdk_import.cmake)
pico_sdk_init()

project(meshmon C CXX ASM)

add_subdirectory(libmeshtastic)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_CURRENT_LIST_DIR})
add_compile_options(-Wall -Wextra -Werror)
add_compile_options(-g -O2)

set(PICO_STACK_SIZE 4096)

add_executable(meshroom
  led.c
  serial.c
  shell.c
  MeshRoom.cxx
  meshroom.cxx)
target_link_libraries(meshroom pico_stdlib libmeshtastic)
pico_enable_stdio_uart(meshroom 0)
pico_add_extra_outputs(meshroom)
