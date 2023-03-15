cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(nixiev_firmware C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(nixiev_firmware
    main.c
)

pico_enable_stdio_usb(nixiev_firmware 1)
pico_enable_stdio_uart(nixiev_firmware 1)

pico_add_extra_outputs(nixiev_firmware)

pico_set_binary_type(nixiev_firmware copy_to_ram)

target_link_libraries(nixiev_firmware pico_stdlib hardware_pwm hardware_adc hardware_spi hardware_rtc pico_multicore hardware_flash)
