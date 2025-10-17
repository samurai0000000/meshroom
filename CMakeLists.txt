# CMakeLists.txt
#
# Copyright (C) 2025, Charles Chiou

cmake_minimum_required(VERSION 3.13)

include(pico-sdk/external/pico_sdk_import.cmake)

project(meshmon VERSION 1.4.12 LANGUAGES C CXX ASM)
include(FreeRTOS_Kernel_import.cmake)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(pico-plat)
add_subdirectory(libmeshtastic)

include_directories(${CMAKE_CURRENT_LIST_DIR})
add_compile_options(-Wall -Wextra -Werror)
add_compile_options(-g -O2)

set(PICO_CXX_ENABLE_EXCEPTIONS 1)
set(PICO_USE_STACK_GUARDS 1)

pico_sdk_init()

include_directories(${CMAKE_CURRENT_LIST_DIR})
add_compile_options(-Wall -Wextra -Werror
  -Wno-unused-variable
  -Wno-unused-but-set-variable
  -Wno-unused-parameter
  -Wno-psabi)
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
  MeshRoom.cxx
  MeshRoomShell.cxx
  meshroom.cxx)
pico_enable_stdio_usb(meshroom 0)
pico_enable_stdio_uart(meshroom 0)
target_include_directories(meshroom PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(meshroom
  pico_stdlib
  tinyusb_device
  tinyusb_board
  hardware_adc
  hardware_spi
  hardware_i2c
  pico_cyw43_arch_none
  FreeRTOS-Kernel-Heap4
  pico-plat
  libmeshtastic
  )
pico_add_extra_outputs(meshroom)
