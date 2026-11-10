# Soft-float ABI sine benchmark (companion of the hard-float cordic_test).
#
# The WHOLE image is compiled with -mfloat-abi=soft (STM32_FLOAT_ABI=soft):
# every float operation is a libgcc __aeabi_f* software call, simulating the
# no-FPU case. Soft and hard ABI objects cannot be mixed in one binary
# (GNU ld rejects the EABI-attribute merge), so this is a separate build.
#
# Benchmarks Soft_Sin (FDLIBM-style sinf, software-emulated float ops) and
# newlib sin() (double software math) over the same 32-angle list as the
# hard build; the READMEs merge the two builds' results.
#
# Build (note the separate build dir; the toolchain file is cached):
#   mkdir -p build-soft && cd build-soft
#   cmake -G Ninja ..
#   ninja

cmake_minimum_required(VERSION 3.13)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/arm-none-eabi-toolchain.cmake")
endif()

# Must be set BEFORE including the board cmake: whole image on the soft ABI.
set(STM32_FLOAT_ABI soft)

project(cordic_test_soft C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}.elf
    main.c
    soft_sin.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/stm32g431_board.cmake)
stm32g431_apply_board(${PROJECT_NAME}.elf "-O1")

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/flash-targets.cmake)
