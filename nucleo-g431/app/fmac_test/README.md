# fmac_test - FMAC (filter math accelerator) benchmark

Benchmarks the STM32G4 **FMAC** (filter math accelerator, q1.15
multiply-accumulate engine) against CPU implementations of the same
workload: a generated **64-tap FIR filter** over a generated **256-sample
q1.15 two-tone stream**, on the **nucleo-g431** board (STM32G431RBT6 @
**170 MHz**). Timing via the DWT cycle counter; one output sample = 64
multiply-accumulates. Follows the `cordic_test` project structure.

Soft and hard float ABI objects cannot be mixed in one binary, so this
project is built twice (results merged below):

| Build | Project | ABI | Methods benchmarked |
| ----- | ------- | --- | ------------------- |
| `build.sh` → `build/` | `fmac_test` | hard (FPU) | FMAC HW, CPU q1.15, float FIR (FPU), double FIR (SW) |
| `build-soft.sh` → `build-soft/` | `fmac_test_soft` | soft (libgcc `__aeabi_f*` SW emulation) | float FIR (soft), double FIR (SW anchor) |

## Methods

1. **FMAC HW** - the FMAC accelerator configured like ST's FMAC_FIR
   examples (function = FIR/convolution, q1.15, clipping enabled, P=64,
   R=0). HAL is used for config/preload/start/stop; the data path is a
   tight register-level pump (polling FMAC_SR, feeding FMAC->WDATA and
   draining FMAC->RDATA) - the HAL polling helper drains only a few words
   per call and would dominate the measurement.
2. **CPU q1.15** - plain-C direct-form FIR: int16 x int16 -> int32 MAC,
   `acc >> 15`, saturate.
3. **FPU float** - plain-C float FIR (hard-float ABI, FPU arithmetic).
4. **Soft float** - the same float FIR compiled `-mfloat-abi=soft`
   (libgcc `__aeabi_f*` software calls - simulates the no-FPU case).
5. **Double** - plain-C double FIR; the chip has no double-precision FPU,
   so all double math is software (`__aeabi_d*` routines).

The input stream (two-tone: 0.5 at 2/64 Fs + 0.25 at 15/64 Fs, q1.15) and
the coefficients (hann-windowed sinc low-pass, fc = Fs/8, unity DC gain,
quantized q1.15) are generated at boot; the console prints the inputs and
per-sample results of every method (captured logs, git-ignored:
`fmac_test_log.txt`, `fmac_test_soft_log.txt`).

## Results (measured on hardware, 170 MHz, GCC 15.3.1, -O1)

Per output sample (64 MACs):

| Method | cycles/sample | time/sample | cycles/MAC |
| ------ | ------------: | ----------: | ---------: |
| **FMAC HW (register pump)** | **132.3** | **0.78 us** | **2.07** |
| CPU q1.15 FIR (plain C) | 785.4 | 4.62 us | 12.27 |
| CPU float FIR (FPU) | 921.4 | 5.43 us | 14.41 |
| CPU float FIR (soft-float emulated) | 6088.4 | 35.81 us | 95.13 |
| CPU double FIR (software) | 10287.5 | 60.51 us | 160.74 |

- FMAC and CPU q1.15 agree within **1 LSB** over all 256 samples (pure
  rounding difference), float agrees with double to 1.5e-07 - every number
  above is computing the same, correct filter.
- The FMAC is **5.9x faster than a plain-C q1.15 FIR** (and it offloads the
  CPU entirely: the host can sleep/DMA while the FMAC crunches). Versus
  float on the FPU it is ~7x faster; versus software-emulated float ~46x.
- 132 cycles/sample includes the CPU pump overhead (status-flag polling +
  one 16-bit write and one 16-bit read per sample via AHB). The MAC engine
  itself runs ~2 cycles/MAC at this clock.

## FMAC gotchas learned on hardware (worth remembering)

- **All FMAC buffer base/size register fields are 8 bits (max 255 words)**.
  An X1 size of 319 silently truncates to 63 (smaller than P!) and the
  filter then never produces output -> HAL polling timeouts. Size the
  buffers so every base and size is <= 255 and feed the stream in chunks.
- **PARAM.R is an extra LEFT shift (gain), 0..7, applied after the
  implicit q1.15 conversion** (`y = (MAC >> 15) << R`, saturating if
  clipping is on). Use R=0 for q1.15-in/q1.15-out with unity-gain
  coefficients. ST's FIR examples use R=1 only because they deliberately
  demonstrate saturation (their reference data is heavily clipped).
- With input/output access = polling, `HAL_FMAC_PollFilterData` returns as
  soon as *either* side completes, so once the input is written it only
  drains a few output words per call - another reason this benchmark pumps
  the registers directly.
- The FIR keeps running across blocks: preload once (zero history), start
  once, then any number of blocks can be pumped; results chain correctly.

## Build / flash

```bash
cd app/fmac_test

# hard-FPU build (FMAC + q1.15 + float + double)
bash build.sh
ninja -C build flash

# soft-float build (soft float FIR + double anchor; separate build dir)
bash build-soft.sh
ninja -C build-soft flash
```

Console: USART2 PA2/PA3, 115200 8-N-1 (ST-LINK VCP). Capture helper at the
repo root: `powershell -File capture_console.ps1 -Seconds 14 -Out <file>`.

## Files

- `src/main.c` - data generation + the four hard-build benchmarks
- `src/fmac_hw.c/.h` - FMAC wrapper (HAL config/preload/start/stop +
  register-level data pump)
- `soft/main.c`, `soft/soft_mac.c/.h` - the soft build (soft float FIR +
  double anchor)
