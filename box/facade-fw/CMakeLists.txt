cmake_minimum_required(VERSION 3.13)
# Octavio Synth front panel firmware (RP2040, Pico SDK 2.x)
set(PICO_BOARD pico)        # same flash (W25Q16, 2 MB) and 12 MHz crystal as the Pico
include(pico_sdk_import.cmake)
project(octavio_facade C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

add_executable(octavio_facade main.c usb_descriptors.c)
pico_generate_pio_header(octavio_facade ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)
target_include_directories(octavio_facade PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(octavio_facade PRIVATE pico_stdlib pico_unique_id hardware_pio hardware_dma hardware_adc
                      tinyusb_device)
target_compile_options(octavio_facade PRIVATE -Wall -Wextra)
pico_add_extra_outputs(octavio_facade)
