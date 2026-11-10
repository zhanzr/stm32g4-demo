# CoreMark 1.0.1 @ 170 MHz — nucleo-g431 (STM32G431RBT6)

CoreMark 1.0.1 (EEMBC, `coremark_1_0_1/`), **10,000 iterations**, on the
**nucleo-g431** board (STM32G431RBT6) at **170 MHz** (HSI 16 MHz → PLL
M=4 N=85 R=2 → SYSCLK, regulator Range 1 BOOST). Compiler-agnostic: the same
sources build with **GNU arm-none-eabi-gcc**, **Keil Arm Compiler 6
(armclang)** or **ST Arm clang** (starm-clang), selected at configure time.
The CoreMark port uses the HAL SysTick 1 kHz tick (`clock()`/`usec()`) from
`src/core_portme.c`, so it works identically on all three compilers.
Follows the `nucleo-f413` `coremark_100m` project.

## Results

Measured on hardware at 170 MHz (hard-float, code from flash, static data):
capture the console while the chip runs the benchmark (it re-runs every
~33 s), and take the last complete `Iterations/Sec` line (earlier lines can
be stale bytes from the previous firmware still draining from the ST-Link
VCP buffer). See the captured `coremark_log.txt` (git-ignored) for a full run.

| Toolchain | Flags                                        | iterations/s | CoreMark/MHz | Time (s) |
| --------- | -------------------------------------------- | ------------ | ------------ | -------- |
| GCC 15.3.1 | `-Ofast -ffp-contract=fast -funroll-all-loops` | **444.504** | **2.615** | 22.50 |
| GCC 15.3.1 + LTO | same + `-flto` | 423.855 | 2.493 | 23.59 |
| ARMCLANG (Keil AC6, Clang 20) | `-Omax -fno-lto` | **519.130** | **3.054** | 19.26 |
| ST Arm clang (starm-clang, Clang 21.1.1) | `-Ofast -ffp-contract=fast -funroll-all-loops` | 395.429 | 2.326 | 25.29 |

- `crcfinal 0x988c` on every run - the same valid CRC as the nucleo-f413
  (STM32F413ZHT6) runs, so the operation is validated and the scores are
  genuine.
- GCC 444.5 iterations/s = **2.615 CoreMark/MHz**, matching ST's published
  **2.62 CoreMark/MHz** for STM32G4 @ 170 MHz (≈445 iterations/s). Keil AC6
  at `-Omax` exceeds it (519 it/s), like it did on the F413.
- The run is fully deterministic: repeat runs report the identical
  22,497 ms total-ticks count (GCC).
- On this board GCC `-flto` **loses** ~5% (423.9 vs 444.5; on the F413 it
  gained ~1%) - no reason to use it. Note CoreMark's per-run CRC forces the
  work to execute, so LTO neither inflates nor cheats it (unlike Dhrystone,
  see `dhry_170m/README.md`).
- The ee_printf flag string printed in the results line is the port's static
  `FLAGS_STR` (the default `-Ofast -ffp-contract=fast -funroll-all-loops`);
  for the armclang `-Omax` row the real flags are those in the table above.
- Captured logs (git-ignored): `coremark_log.txt` (gcc),
  `coremark_log_armclang.txt`, `coremark_log_starm.txt`,
  `coremark_log_gcc_lto.txt`.

## Build / flash

```bash
cd app/coremark_170m
bash build.sh          # == mkdir -p build && cd build && cmake -G Ninja .. && ninja
ninja flash            # probe-rs download + reset over ST-Link SWD
```

Optional toolchains / tuning (separate build dir per toolchain;
`CMAKE_TOOLCHAIN_FILE` is cached at configure time):

```bash
# Keil AC6 at -Omax: BENCH_OPT_C is applied to the C files only
# (arm-none-eabi-gcc rejects -Omax at the asm/link step) and -fno-lto is
# required because -Omax would otherwise emit LTO objects GNU ld cannot link.
cmake -G Ninja -DSTM32_TOOLCHAIN=armclang '-DBENCH_OPT=' '-DBENCH_OPT_C=-Omax -fno-lto' -B build-armclang ..
ninja -C build-armclang

# gcc + LTO (valid here; contrast with Dhrystone)
cmake -G Ninja -DSTM32_TOOLCHAIN=gcc -DSTM32_LTO=ON -B build-gcc-lto ..

# ST Arm clang (starm-clang) + LTO
cmake -G Ninja -DSTM32_TOOLCHAIN=starm-clang -B build-starm ..
```

## Files

- `src/main.c` - board bring-up + repeated `coremark_main()` calls
- `src/core_portme.c/.h` - CoreMark port (HAL SysTick timing, static memory,
  PERFORMANCE_RUN, ITERATIONS=10000)
- `coremark_1_0_1/` - unmodified CoreMark 1.0.1 sources (EEMBC license)
- `src/utils.c/.h`, `src/custom_def.h` - tick/compiler-name glue

## Console

Results are printed over the board's console UART (USART2 PA2/PA3,
115200 8-N-1 - ST-LINK VCP). A capture helper lives at the repo root:
`capture_console.ps1` (opens the VCP, resets the board, records N seconds).
