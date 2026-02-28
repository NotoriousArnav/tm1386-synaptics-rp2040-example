# SPDX-License-Identifier: GPL-3.0-or-later
#
# lib/ps2/CMakeLists.txt — Reusable PS/2 host driver library for RP2040
#
# To use in another project:
#   1. Copy the lib/ps2/ folder into your project
#   2. In your top-level CMakeLists.txt, add:
#        add_subdirectory(lib/ps2)
#        target_link_libraries(your_target ps2)
#   3. In your source:
#        #include "ps2.hpp"

add_library(ps2 INTERFACE)

target_sources(ps2 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ps2.cpp
)

target_include_directories(ps2 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Compile ps2.pio → ps2.pio.h at build time
pico_generate_pio_header(ps2 ${CMAKE_CURRENT_LIST_DIR}/ps2.pio)

target_link_libraries(ps2 INTERFACE
    pico_stdlib
    hardware_pio
    hardware_gpio
    hardware_clocks
)
