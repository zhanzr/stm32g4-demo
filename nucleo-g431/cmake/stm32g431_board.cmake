# Shared STM32G431RBTx board layer for the "nucleo-g431" board projects
# (Nucleo-64, STM32G431RBT6 at its 170 MHz maximum from the 16 MHz HSI -
# no crystal is fitted on the Nucleo-64).
# The board support (clock init to 170 MHz, LED GPIO, newlib stubs, startup,
# linker script) plus the STM32G4 HAL driver sources are attached to a target
# with stm32g431_apply_board().
#
# Usage (from a project CMakeLists.txt, after add_executable()):
#   include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32g431_board.cmake)
#   stm32g431_apply_board(${PROJECT_NAME}.elf "-Ofast")
#
# Requires the project to enable ASM (project(X C ASM)).

# Vendored HAL/CMSIS (repo root `drivers/`). To use the full STM32Cube_FW_G4
# package instead, point this at its root (contains Drivers/).
#   e.g. -DSTM32_G4_HAL_ROOT="C:/Users/user1/STM32Cube/Repository/STM32Cube_FW_G4_V1.6.3"
set(STM32_G4_HAL_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../drivers" CACHE PATH
    "Root of the STM32G4 HAL+ CMSIS (vendored `drivers/` or a full STM32Cube_FW_G4 package)")

set(BOARD_DIR ${CMAKE_CURRENT_LIST_DIR}/../board)
# Linker script. Default is the board's standard one; a project may point this
# at its own script (e.g. to run benchmark code from a RAM region) by
# setting BOARD_LINKER_SCRIPT before including this file.
if(NOT BOARD_LINKER_SCRIPT)
    set(BOARD_LINKER_SCRIPT ${BOARD_DIR}/stm32g431rb.ld)
endif()
set(STM32_G4_HAL_INC ${STM32_G4_HAL_ROOT}/STM32G4xx_HAL_Driver/Inc)
set(STM32_G4_HAL_SRC ${STM32_G4_HAL_ROOT}/STM32G4xx_HAL_Driver/Src)
set(STM32_G4_CMSIS_DEV ${STM32_G4_HAL_ROOT}/CMSIS/Device/ST/STM32G4xx/Include)
set(STM32_G4_CMSIS_CORE ${STM32_G4_HAL_ROOT}/CMSIS/Include)

# Floating-point ABI for the whole image: "hard" (FPU arithmetic + VFP
# argument registers; the default) or "soft" (everything on the software-
# float ABI, e.g. cordic_test's soft-sinf build). Soft and hard objects
# cannot be mixed in one binary (GNU ld rejects the EABI-attribute merge),
# so a soft build must set this before including this file.
if(NOT STM32_FLOAT_ABI)
    set(STM32_FLOAT_ABI hard)
endif()

function(stm32g431_apply_board TGT OPT)
    separate_arguments(OPT_LIST NATIVE_COMMAND "${OPT}")

    # GCC-only warning switches; keep clang-based toolchains clean.
    if(STM32_ARMCLANG OR STM32_STARM_CLANG)
        set(_WARN_FLAGS -Wall -Wno-unused-command-line-argument)
    else()
        set(_WARN_FLAGS
            -Wall
            -Wno-unused-but-set-variable -Wno-unused-function
            -Wno-unused-variable -Wno-unused-parameter -Wno-maybe-uninitialized)
    endif()

    target_sources(${TGT} PRIVATE
        ${BOARD_DIR}/board.c
        ${BOARD_DIR}/stm32g4xx_hal_msp.c
        ${BOARD_DIR}/stm32g4xx_it.c
        ${BOARD_DIR}/swv_printf.c
        ${BOARD_DIR}/uart_printf.c
        ${BOARD_DIR}/syscalls.c
        ${BOARD_DIR}/startup_stm32g431xx.s
        ${BOARD_DIR}/system_stm32g4xx.c
        ${STM32_G4_HAL_SRC}/stm32g4xx_hal.c
        ${STM32_G4_HAL_SRC}/stm32g4xx_hal_adc.c
        ${STM32_G4_HAL_SRC}/stm32g4xx_hal_adc_ex.c
        ${STM32_G4_HAL_SRC}/stm32g4xx_hal_cortex.c
        ${STM32_G4_HAL_SRC}/stm32g4xx_hal_cordic.c
        ${STM32_G4_HAL_SRC}/stm32g4xx_hal_dma.c
        ${STM32_G4_HAL_SRC}/stm32g4xx_hal_flash.c
        ${STM32_G4_HAL_SRC}/stm32g4xx_hal_flash_ex.c
        ${STM32_G4_HAL_SRC}/stm32g4xx_hal_fmac.c
        ${STM32_G4_HAL_SRC}/stm32g4xx_hal_gpio.c
        ${STM32_G4_HAL_SRC}/stm32g4xx_hal_i2c.c
        ${STM32_G4_HAL_SRC}/stm32g4xx_hal_pwr.c
        ${STM32_G4_HAL_SRC}/stm32g4xx_hal_pwr_ex.c
        ${STM32_G4_HAL_SRC}/stm32g4xx_hal_rcc.c
        ${STM32_G4_HAL_SRC}/stm32g4xx_hal_rcc_ex.c
        ${STM32_G4_HAL_SRC}/stm32g4xx_hal_spi.c
        ${STM32_G4_HAL_SRC}/stm32g4xx_hal_uart.c
        ${STM32_G4_HAL_SRC}/stm32g4xx_hal_uart_ex.c
    )

    target_include_directories(${TGT} PRIVATE
        ${BOARD_DIR}
        ${STM32_G4_HAL_INC}
        ${STM32_G4_HAL_INC}/Legacy
        ${STM32_G4_CMSIS_DEV}
        ${STM32_G4_CMSIS_CORE}
    )

    # armclang has no bundled libc headers: point it at the GNU newlib include
    # dir so <stdio.h>/<string.h>/... resolve to the same newlib we link.
    if(STM32_ARMCLANG)
        target_include_directories(${TGT} SYSTEM PRIVATE
            "${ARM_GCC_ROOT}/arm-none-eabi/include"
        )
    endif()

    target_compile_definitions(${TGT} PRIVATE STM32G431xx USE_HAL_DRIVER)

    target_compile_options(${TGT} PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=${STM32_FLOAT_ABI} -mfpu=fpv4-sp-d16
        ${OPT_LIST} -g
        -ffunction-sections -fdata-sections ${_WARN_FLAGS}
    )

    # starm-clang links with LLD by default; use -Xlinker for linker flags.
    if(STM32_STARM_CLANG)
        set(_STARM_SYSROOT "${STARM_ROOT}/lib/clang-runtimes/newlib")
        set(_STARM_LIBDIR "${_STARM_SYSROOT}/arm-none-eabi/armv7m_hard_fpv4_sp_d16_exn_rtti_unaligned_size/lib")
        set(_LDFLAGS "-nostartfiles")
        set(_LDFLAGS "${_LDFLAGS} -Xlinker -T -Xlinker ${BOARD_LINKER_SCRIPT}")
        set(_LDFLAGS "${_LDFLAGS} -Xlinker --gc-sections")
        set(_LDFLAGS "${_LDFLAGS} -Xlinker -Map=${PROJECT_NAME}.map")
        set(_LDFLAGS "${_LDFLAGS} -L${_STARM_LIBDIR}")
        set(_LDFLAGS "${_LDFLAGS} -Xlinker --start-group")
        set(_LDFLAGS "${_LDFLAGS} ${_STARM_LIBDIR}/libclang_rt.builtins.a -lc -lm")
        set(_LDFLAGS "${_LDFLAGS} -Xlinker --end-group")
    else()
        set(_LDFLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=${STM32_FLOAT_ABI} -mfpu=fpv4-sp-d16 ${OPT}")
        set(_LDFLAGS "${_LDFLAGS} -Wl,--gc-sections -nostartfiles")
        set(_LDFLAGS "${_LDFLAGS} -Wl,-Map=${PROJECT_NAME}.map")
        set(_LDFLAGS "${_LDFLAGS} -T ${BOARD_LINKER_SCRIPT}")
    endif()

    set_target_properties(${TGT} PROPERTIES
        LINK_FLAGS "${_LDFLAGS}"
    )

    # libc/libm must come AFTER the objects (CMake places target libraries
    # there): archive members are only extracted for symbols still undefined
    # at the point the archive is scanned, so -lc/-lm in LINK_FLAGS - which
    # precede the objects - can never satisfy object references (e.g. sinf).
    target_link_libraries(${TGT} PRIVATE c m)

    # newlib/libgcc's thumb/v7e-m+fp/hard multilib objects are built with
    # -fshort-enums and lack .note.GNU-stack, so a GNU ld link spews ~60
    # benign warnings. Silence them (sizes match the ARM EABI defaults our
    # objects use, so this is noise, not an ABI error). LLD (starm-clang)
    # does not support these GNU-ld-only switches, so skip them there.
    if(NOT STM32_STARM_CLANG)
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Wl,--no-enum-size-warning -Wl,--no-wchar-size-warning -Wl,--no-warn-execstack")
    endif()

    # Link-time optimization (GCC and starm-clang).
    # GCC: -flto via GNU ld's lto plugin.
    # starm-clang: -flto=full via LLD's native LTO (bitcode consumed directly).
    # Keil armclang: LTO impossible — emits LLVM bitcode that GNU ld cannot use.
    if(STM32_LTO AND STM32_STARM_CLANG)
        # Full LLVM LTO: compile to bitcode (-flto=full), link with LLD's LTO.
        # Keep -ffat-lto-objects so syscalls.c-style -fno-lto sources still
        # produce relocatable objects. The aggressive pass thresholds come from
        # ST's OmaxLTO.cfg (inlined/unrolled via -Wl,-plugin-opt at link time,
        # see below). C++-only devirt flags are omitted (pure-C benchmarks).
        target_compile_options(${TGT} PRIVATE
            -flto=full -ffat-lto-objects
        )
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -flto=full")
        # Pass aggressive LTO plugin opts (from ST's OmaxLTO.cfg).
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-extra-LTO-loop-unroll=true")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-inline-threshold=500")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-unroll-threshold=450")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-unroll-partial-threshold=450")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-unroll-max-iteration-count-to-analyze=20")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-lsr-complexity-limit=1073741823")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-force-attribute=main:norecurse")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-enable-dfa-jump-thread")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-enable-loop-flatten")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-enable-unroll-and-jam")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-enable-inline-memcpy-ld-st")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-enable-loop-versioning-licm")
        # syscalls.c: tiny retarget layer, compile without LTO to avoid
        # newlib syscall-stub resolution issues (same rationale as GCC).
        set_source_files_properties(${BOARD_DIR}/syscalls.c PROPERTIES
            COMPILE_OPTIONS "-fno-lto")
    elseif(STM32_LTO AND NOT STM32_ARMCLANG AND NOT STM32_STARM_CLANG)
        target_compile_options(${TGT} PRIVATE -flto)
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY LINK_FLAGS " -flto")
        set_source_files_properties(${BOARD_DIR}/syscalls.c PROPERTIES
            COMPILE_OPTIONS "-fno-lto")
    endif()
endfunction()
