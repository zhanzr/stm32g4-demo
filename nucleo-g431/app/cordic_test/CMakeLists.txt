# CORDIC sine benchmark for the Nucleo-G431RB board (STM32G431RBT6 @ 170 MHz).
# Hard-float ABI build. Compares four sin(x) implementations over a generated
# 32-angle input list:
#   1. CORDIC      - the STM32G4 hardware CORDIC IP (HAL polling mode)
#   2. CMSIS DSP   - arm_sin_f32() (vendored CMSIS-DSP fast-math subset)
#   3. FPU float   - newlib sinf() (hard-float ABI, FPU arithmetic)
#   4. Double      - newlib sin() (double software math on this chip)
# The soft-float (no-FPU simulated) sinf variant lives in the separate
# soft-float-ABI build in soft/ (see ../build-soft.sh) - soft and hard ABI
# objects cannot be mixed in one binary; the READMEs merge the results.
# Timing via the DWT cycle counter.
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

project(cordic_test C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(DSP ${CMAKE_CURRENT_SOURCE_DIR}/../../../drivers/CMSIS/DSP)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/cordic_hw.c
    ${DSP}/Source/FastMathFunctions/arm_sin_f32.c
    ${DSP}/Source/CommonTables/arm_common_tables.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32g431_board.cmake)
stm32g431_apply_board(${PROJECT_NAME}.elf "-O1")

# CMSIS-DSP fast-math subset (arm_sin_f32 + its sinTable_f32)
target_include_directories(${PROJECT_NAME}.elf PRIVATE
    src
    ${DSP}/Include
)
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE ARM_MATH_CM4)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
