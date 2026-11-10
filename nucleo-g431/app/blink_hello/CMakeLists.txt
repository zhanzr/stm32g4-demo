# LED blink demo for the Nucleo-G431RB board (STM32G431RBT6). Blinks the
# onboard user LED (LD2 = PA5, high-active) and prints a banner plus a
# heartbeat message over USART2 (PA2/PA3) at 115200 baud (ST-Link VCP).
# Runs at 170 MHz from the 16 MHz HSI (PLL, VOS Range 1 BOOST).
#
# Pico-style workflow:
#   mkdir -p build && cd build
#   cmake -G Ninja ..
#   ninja           # builds .elf / .hex / .bin
#   ninja flash     # programs the board (probe-rs, ST-Link SWD)

cmake_minimum_required(VERSION 3.13)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/arm-none-eabi-toolchain.cmake")
endif()

project(blink_hello C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/adc_internal.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32g431_board.cmake)
stm32g431_apply_board(${PROJECT_NAME}.elf "-O1")


include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
