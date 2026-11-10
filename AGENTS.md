# AGENTS.md

Guidance for AI agents (and humans) working in this repo. Keep it brief; the
authoritative docs already live in each board's `README.md` and the root
`README.md`.

## What this repo is

Bare-metal firmware for **STM32G4-series** boards. It is a **multi-board /
multi-chip** repo: each board lives in its own `<name>-<chip>` folder
(`nucleo-g431`) and is fully self-contained:

- `board/` — clock init, LEDs, console, startup assembly, linker script,
  `stm32g4xx_hal_conf.h`, `stm32g4xx_hal_msp.c`, `stm32g4xx_it.c`, newlib stubs.
- `cmake/` — toolchain files, the board-apply helper, flash targets, OpenOCD cfg.
- `app/` — the actual projects; each has a `build.sh`, a `CMakeLists.txt`,
  and a `src/` directory.
- Vendored HAL/CMSIS lives in the **repo root** `drivers/` (all boards share it).

Per-board hardware/clock/console facts differ — **always read the board's own
`README.md`** before touching or writing a project for it (e.g. `nucleo-g431`
has no HSE crystal — clocks come from the 16 MHz HSI via PLL to the 170 MHz
family maximum, which requires the PWR Range 1 BOOST regulator; the console is
USART2-PA2/PA3 VCP and the only user LED is LD2=PA5, high-active).

## Build & flash workflow

Pico-style **CMake + Ninja**, run per-project. From a project dir
(`app/blink_hello`, etc.):

```bash
bash build.sh        # == mkdir -p build && cd build && cmake -G Ninja .. && ninja
ninja flash          # probe-rs download + reset over SWD
ninja flash-ocd      # alternative: flash via OpenOCD
```

The `build/` directories under each project are **generated** (and git-ignored);
do not hand-edit them.

- Toolchain: GNU arm-none-eabi-gcc (default) **or** Keil AC6 `armclang` via
  `-DSTM32_TOOLCHAIN=armclang`. CMake/Ninja come from the MSYS2 mingw64
  environment (added to `PATH` by `build.sh` when missing).
- Flash uses **probe-rs** by default (auto-detects the probe); OpenOCD is the
  alternative. Pin a specific probe with `-DDEBUG_PROBE=<selector>` at configure
  time (the nucleo-g431 board CMake already pins its own ST-LINK serial).
- Console capture: `powershell -File capture_console.ps1 [-Seconds N] [-Out <file>]`
  opens the ST-LINK VCP, resets the board and records the UART output
  (benchmarks re-run in a loop; take the last complete result line).

## Project structure conventions

When adding a new project, follow the pattern of an existing one
(e.g. `nucleo-g431/app/blink_hello`):

- `CMakeLists.txt`:
  - `cmake_minimum_required(VERSION 3.13)`; set `CMAKE_TOOLCHAIN_FILE` to
    `../../cmake/arm-none-eabi-toolchain.cmake` if not already set.
  - `project(<name> C ASM)` — **ASM is required** (startup `.s`).
  - `add_executable(<name>.elf src/... )`.
  - `include(../../cmake/stm32g431_board.cmake)` and call the board-apply
    function with the optimization flag, e.g.
    `stm32g431_apply_board(${PROJECT_NAME}.elf "-O1")`.
  - Add the objcopy `.hex`/`.bin` POST_BUILD command and
    `include(../../cmake/flash-targets.cmake)` (handled by
    `flash-targets.cmake` itself — see the existing project).
- `build.sh`: copy the existing one verbatim (it is generic).
- `README.md`: document hardware, build, flash, console.

Board code lives in `board/`, **not** per-project. New shared helper source
belongs there and is pulled in by the board-apply CMake function.

## Conventions & gotchas

- HAL vendor tree is shared across boards in root `drivers/`; `STM32_G4_HAL_ROOT`
  defaults to `../../drivers`. To use the full STM32Cube_FW_G4 package instead,
  pass `-DSTM32_G4_HAL_ROOT` at configure time.
- Compile definitions set by the board layer: `STM32G431xx` (or the G4 part)
  and `USE_HAL_DRIVER`.
- `-ffunction-sections -fdata-sections` + `--gc-sections` are on by default.
- **LTO** (`-DSTM32_LTO=ON`) is GCC-only; armclang ignores it (LLVM bitcode
  cannot be consumed by GNU ld). When LTO is on, `syscalls.c` is forcibly
  compiled `-fno-lto` — do not remove that.
- Console output goes through `uart_printf` (the board layer), not `printf`
  re-implementation; `printf` itself is retargeted via `_write()` →
  `UART_PutChar()` (and `bench_printf` exists for armclang builds, see
  `cmake/printf_rename.h`).
- Do not add code comments beyond what's needed; the existing sources use
  sparse explanatory comments only where non-obvious.
- There is no test suite in this repo; verification is by building + flashing
  a target board (or just `bash build.sh` to confirm it compiles clean).
