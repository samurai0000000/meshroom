# CMakeLists.txt
#
# Copyright (C) 2025, Charles Chiou

cmake_minimum_required(VERSION 3.13)

include (pico-sdk/external/pico_sdk_import.cmake)

project(meshmon VERSION 1.4.8 LANGUAGES C CXX ASM)
include(FreeRTOS_Kernel_import.cmake)

set(PICO_CXX_ENABLE_EXCEPTIONS 1)
set(PICO_USE_STACK_GUARDS 1)
set(PICO_STACK_SIZE 2048)
set(PICO_CORE1_STACK_SIZE 2048)
set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 500)

pico_sdk_init()

add_subdirectory(libmeshtastic)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_CURRENT_LIST_DIR})
add_compile_options(-Wall -Wextra -Werror -Wno-unused-variable -Wno-psabi)
add_compile_options(-g -Os)

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
  serial.c
  MeshRoom.cxx
  MeshRoomShell.cxx
  meshroom.cxx)
pico_enable_stdio_usb(meshroom 1)
pico_enable_stdio_uart(meshroom 0)
target_compile_definitions(meshroom PRIVATE PICO_STACK_SIZE=${PICO_STACK_SIZE})
target_include_directories(meshroom PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(meshroom
  libmeshtastic
  FreeRTOS-Kernel-Heap4
  pico_stdlib
  pico_multicore
  hardware_adc
  )
pico_add_extra_outputs(meshroom)
