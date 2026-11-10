# Dhrystone 2.1 @ 170 MHz — nucleo-g431 (STM32G431RBT6)

Classic Dhrystone 2.1 (dhry_1.c / dhry_2.c / dhry.h), **2,000,000 runs**, on
the **nucleo-g431** board (STM32G431RBT6) clocked at **170 MHz** (HSI 16 MHz,
PLL M=4 N=85 R=2 → SYSCLK, regulator Range 1 BOOST). Compiler-agnostic: the
same sources build with either **GNU arm-none-eabi-gcc** or **Keil Arm
Compiler 6 (armclang)**, selected at configure time. Follows the
`nucleo-f413` `dhry_100m` project.

## Results

Measured on hardware at 170 MHz (hard-float, code from flash, static data):
capture the console while the chip runs the benchmark (it re-runs every
~16 s), and take the last complete `Dhrystones per Second` line. See the
captured `dhrystone_log.txt` (git-ignored) for a full run.

| Toolchain | Flags                                      | Dhrystones/s | DMIPS  | DMIPS/MHz |
| --------- | ------------------------------------------ | ------------ | ------ | --------- |
| GCC 15.3.1 | `-Ofast -ffp-contract=fast -funroll-loops` | **358,551** | 204.0 | **1.200** |

All runs print correct final values (Int_Glob=5, Arr_2_Glob[8][7] =
runs+10, …), so the score is genuine.

> ⚠ **Do not use LTO for Dhrystone.** GCC `-flto` sees the whole program and
> hoists the loop-invariant work out of the timed loop, inflating the score
> (~2.1× on the sibling nucleo-f413 board) while still passing the
> final-value check. The LTO number is meaningless. Evidence and analysis:
> `LTO_on_dhrystone.md` in the f4-demo repo's `nucleo-f413/app/dhry_100m`.

## Notes

- 1.20 DMIPS/MHz is the expected range for a Cortex-M4F running from flash on
  this family: the STM32G4 flash interface has no ART accelerator (unlike
  F4), only instruction prefetch + instruction cache, so flash-fetch bound
  code scales sub-linearly with the 170 MHz core clock (F413 reached 1.34
  DMIPS/MHz @ 100 MHz on the same core with ART).
- Run time: 2M runs in ~5.6 s per pass, then a 10 s pause before re-running.

## Build / flash

```bash
cd app/dhry_170m
bash build.sh          # == mkdir -p build && cd build && cmake -G Ninja .. && ninja
ninja flash            # probe-rs download + reset over ST-Link SWD
```

Optional toolchains (separate build dir per toolchain; `CMAKE_TOOLCHAIN_FILE`
is cached at configure time):

```bash
cmake -G Ninja -DSTM32_TOOLCHAIN=armclang -B build-armclang ..
cmake -G Ninja -DSTM32_TOOLCHAIN=gcc -DSTM32_LTO=ON -B build-gcc-lto ..   # evidence only, see warning
ninja -C build-armclang
```

## Files

- `src/main.c` - board bring-up + repeated `dhry_main()` calls
- `src/dhry_1.c`, `src/dhry_2.c`, `src/dhry.h` - Dhrystone 2.1 (2M runs,
  SysTick-millisecond timing, DMIPS/MHz reported with the 1757 loop factor)
- `src/utils.c/.h`, `src/custom_def.h` - tick/compiler-name glue

## Console

Results are printed over the board's console UART (USART2 PA2/PA3,
115200 8-N-1 - ST-LINK VCP). A capture helper lives at the repo root:
`capture_console.ps1` (opens the VCP, resets the board, records N seconds).
