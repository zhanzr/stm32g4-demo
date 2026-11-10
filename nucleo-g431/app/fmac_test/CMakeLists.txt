# FMAC (filter math accelerator) benchmark for the Nucleo-G431RB board
# (STM32G431RBT6 @ 170 MHz). Hard-float ABI build. Benchmarks a generated
# 64-tap FIR filter over a generated 256-sample q1.15 two-tone stream:
#   1. FMAC HW    - the STM32G4 FMAC accelerator (q1.15 MACs, HAL polling)
#   2. CPU q1.15  - plain-C integer FIR
#   3. FPU float  - plain-C float FIR (hard-float ABI, FPU arithmetic)
#   4. Double     - plain-C double FIR (software double math on this chip)
# The soft-float (no-FPU simulated) float FIR lives in the separate
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

project(fmac_test C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/fmac_hw.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32g431_board.cmake)
stm32g431_apply_board(${PROJECT_NAME}.elf "-O1")

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
