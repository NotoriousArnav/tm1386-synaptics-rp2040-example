# SPDX-License-Identifier: GPL-3.0-or-later
#
# CMakeLists.txt — PS/2 Trackpad (TM1386) reader for Raspberry Pi Pico

cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(pico-touchpad-tm1386 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# PS/2 driver library (reusable — see lib/ps2/CMakeLists.txt)
add_subdirectory(lib/ps2)

# Application
add_executable(main
    src/main.cpp
)

target_link_libraries(main
    pico_stdlib
    ps2
)

# Output printf over USB serial (CDC), not UART
pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 0)

# Generate .uf2, .hex, .bin etc.
pico_add_extra_outputs(main)
