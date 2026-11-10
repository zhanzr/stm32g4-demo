# G4 demo (multi-board)

Bare-metal firmware projects and tooling for multiple **STM32G4-series**
boards/chips. Each board lives in its own folder with a self-contained
`board/` (clock, LEDs, console, startup, linker script), `cmake/` (toolchain +
board helpers) and `app/` (projects). Board-level docs — hardware, clock tree,
build/flash/console — live in each board folder's README.

## Boards

| Board         | What it is                                                                        |
| ------------- | --------------------------------------------------------------------------------- |
| `nucleo-g431/`| Nucleo-64, STM32G431RBT6 @ 170 MHz (HSI PLL), 128 KB flash / 32 KB SRAM, 1 LED (LD2=PA5), USART2 → ST-Link VCP console (see its README) |

The `-<chip>` suffix in board folder names keeps it a multi-board/**multi-chip**
repo: e.g. a `nucleo-g474` would sit next to `nucleo-g431`.

## Vendored HAL / CMSIS

The STM32G4 **HAL driver + CMSIS** are vendored in the repo root `drivers/`
(trimmed subset of the official `STM32Cube_FW_G4` package):

```
drivers/
├── CMSIS/
│   ├── Include/                        CMSIS core headers
│   └── Device/ST/STM32G4xx/Include/    STM32G4 device headers
└── STM32G4xx_HAL_Driver/
    ├── Inc/ (+ Legacy)                 HAL headers
    └── Src/ (all HAL modules)          HAL sources
```

Builds use this by default (`-DSTM32_G4_HAL_ROOT` defaults to `../../drivers`).
The trimmed tree covers everything these projects compile; it does **not**
include the BSP, DSP, middleware, projects, docs, or `.chm` manuals.

To use the **full** official package instead (e.g. to pull in something not
vendored), point `STM32_G4_HAL_ROOT` at its root when configuring:

```bash
cmake -G Ninja -DSTM32_G4_HAL_ROOT="C:/Users/user1/STM32Cube/Repository/STM32Cube_FW_G4_V1.6.3" ..
```

The full package is STM32CubeMX → "Manage embedded software packages" →
STM32Cube MCU Package → "STM32Cube FW_G4 V1.6.3", installed under
`<STM32Cube>/Repository/STM32Cube_FW_G4_V1.6.3`.

## Toolchain / environment

* GNU arm-none-eabi-gcc (default) or Keil AC6 armclang (`-DSTM32_TOOLCHAIN=armclang`).
* CMake + Ninja (Pico-style; MSYS2 mingw64 `build.sh` adds them to `PATH`).
* probe-rs (SWD flashing) + OpenOCD (alternative).

## Build configuration

Configure and build each project from its own directory
(`<board>/app/<project>`). No absolute paths are needed — the build directory
is a relative subfolder of the project.

```bash
cd nucleo-g431/app/blink_hello

# default: gcc + the project's hard-coded optimization level
bash build.sh          # == mkdir -p build && cd build && cmake -G Ninja .. && ninja
ninja flash            # program via probe-rs (ST-Link SWD)

# other toolchains: use one build dir per toolchain
# (CMAKE_TOOLCHAIN_FILE is cached at configure time)
cmake -G Ninja -S . -B build-armclang -DSTM32_TOOLCHAIN=armclang
ninja -C build-armclang
```

**Optimization levels** are passed to the board-apply CMake function in each
project's `CMakeLists.txt` (e.g. `-O1` for simple demos like `blink_hello`,
`-O2`/`-O3` for peripheral-heavy apps).
