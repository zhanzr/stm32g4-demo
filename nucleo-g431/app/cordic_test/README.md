# cordic_test - sine benchmark (hardware CORDIC vs CMSIS DSP vs FPU vs soft-float vs double)

Benchmarks **sin(x)** over a generated list of **32 angles** (0..2*pi, step
2*pi/32) on the **nucleo-g431** board (STM32G431RBT6 @ **170 MHz**), timing
with the DWT cycle counter. The inputs and all per-angle results are printed
over the console (see the captured `cordic_test_log.txt` /
`cordic_test_soft_log.txt`, git-ignored).

Soft and hard float ABI objects cannot be mixed in one binary (GNU ld
rejects the EABI-attribute merge), so this project is built twice:

| Build | Project | ABI | Methods benchmarked |
| ----- | ------- | --- | ------------------- |
| `build.sh` → `build/` | `cordic_test` | hard (FPU) | CORDIC HW, CMSIS DSP, sinf (FPU), sin (double SW) |
| `build-soft.sh` → `build-soft/` | `cordic_test_soft` | soft (libgcc `__aeabi_f*` SW emulation) | sinf (soft-emulated), sin (double SW, anchor) |

## Methods

1. **CORDIC** - the STM32G4 hardware CORDIC IP, configured like ST's
   `CORDIC_Sin_DMA` example (function = sine, precision = 6 cycles,
   q1.31 in/out, 2^31 = pi), HAL polling mode (`HAL_CORDIC_Calculate`).
2. **CMSIS DSP** - `arm_sin_f32()` from the vendored CMSIS-DSP fast-math
   subset (512-entry table + linear interpolation).
3. **FPU float** - newlib `sinf()`; float arithmetic on the FPU
   (hard-float ABI).
4. **Soft float** - FDLIBM-style float sine (`soft/soft_sin.c`: range
   reduction to [-pi/4, pi/4] + 4-term minimax polynomial) compiled with
   `-mfloat-abi=soft`, so every float op is a libgcc `__aeabi_f*` software
   call - simulates the no-FPU case.
5. **Double** - newlib `sin()`; the chip has no double-precision FPU, so
   all double math is software (`__aeabi_d*` routines) in both builds.

## Results (measured on hardware, 170 MHz, GCC 15.3.1, -O1)

| Method | cycles/sin | time/sin | max abs error vs sin(double) |
| ------ | ----------: | -------: | ---------------------------- |
| **CORDIC HW (HAL polling)** | **92.3** | **0.54 us** | 8.8e-07 |
| CMSIS `arm_sin_f32` | 65.6 | 0.39 us | 3.4e-07 |
| `sinf` (newlib, FPU) | 199.3 | 1.17 us | 3.7e-08 |
| `sinf` (soft-float emulated) | 1139.2 | 6.70 us | 2.9e-05 |
| `sin` (newlib double, software) | 2792.7 / 2737.6 | 16.4 us | (reference) |

- The hardware **CORDIC computes a sine in ~0.54 us** - 2.2x faster than the
  CMSIS table lookup, 4x faster than `sinf`, 12x faster than soft-float and
  **30x faster than software double** - at q1.31 fixed-point accuracy
  (~9e-7 abs; CMSIS's interpolation is slightly more accurate but is an
  approximation too).
- CORDIC's cycles include the full HAL polling API overhead
  (`HAL_CORDIC_Calculate`: per-sample flag polling + argument/result
  register I/O); the raw IP needs only 6 wait states per result, so a
  DMA-driven or register-level loop would be even faster.
- `sin` (double) is software in both builds and the two builds agree
  (2792.7 hard-ABI vs 2737.6 soft-ABI cycles), which anchors the merged
  comparison.
- The soft-float sine (1139 cycles) is the FDLIBM algorithm; newlib's sinf
  uses the same algorithm class, so ~6x software-vs-FPU is the intrinsic
  cost of emulating float arithmetic.

## Build / flash

```bash
cd app/cordic_test

# hard-FPU build (CORDIC + CMSIS + sinf + sin)
bash build.sh          # == mkdir -p build && cd build && cmake -G Ninja ../
ninja flash            # (run from build/)

# soft-float build (soft sinf + sin; separate build dir, separate project)
bash build-soft.sh
ninja -C build-soft flash
```

Console: USART2 PA2/PA3, 115200 8-N-1 (ST-LINK VCP). Capture helper at the
repo root: `powershell -File capture_console.ps1 -Seconds 14 -Out <file>`.

## Files

- `src/main.c` - input-list generation + the four hard-build benchmarks
- `src/cordic_hw.c/.h` - hardware CORDIC wrapper (ST-example configuration)
- `soft/main.c` - the soft build's benchmark main
- `soft/soft_sin.c/.h` - FDLIBM-style soft-float sine
- CMSIS DSP subset vendored at `drivers/CMSIS/DSP` (`arm_sin_f32.c` +
  `arm_common_tables.c` + headers, from STM32Cube_FW_G4_V1.6.3)
