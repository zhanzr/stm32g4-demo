# ST7796S 320x480 IPS LCD + FT6336U capacitive touch demo
# ("ips395-st7796-captouch") for the "nucleo-g431" board (STM32G431RBT6
# @ 170 MHz). 4-wire SPI display (same wiring as sx395hvga40p-01-v01):
#   SCL = PB13, SDA = PB15, DC = PB2, RST = PB1, CS = PB14, BL = PB11
#   (TIM2_CH4 PWM).
# FT6336U capacitive touch on I2C1:
#   T_SCL = PB8 (I2C1_SCL AF4), T_SDA = PB9 (I2C1_SDA AF4),
#   T_INT = PC6 (input, pull-up), T_RST = PC8 (GPIO out).
# RGB565 in 4-line serial (COLMOD 0x55): two bytes per pixel.
# Two drive methods like sx395hvga40p-01-v01: soft bit-bang and HW SPI2
# (DMA raster bursts via DMA1 Ch1 / DMAMUX request 13 = SPI2_TX).
#
# Pico-style workflow:
#   bash build.sh          # == mkdir build && cd build && cmake -G Ninja .. && ninja
#   ninja flash            # programs the board (probe-rs, ST-Link SWD)

cmake_minimum_required(VERSION 3.13)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/arm-none-eabi-toolchain.cmake")
endif()

project(ips395-st7796-captouch C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/interface.c
    src/lcd.c
    src/lcd/lcd_fonts.c
    src/lcd/lcd_font_1608.c
    src/touch.c
    src/backlight.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32g431_board.cmake)
stm32g431_apply_board(${PROJECT_NAME}.elf "-O1")

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    src
    src/blockwrite
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
