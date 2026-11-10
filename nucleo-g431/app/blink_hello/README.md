# blink_hello - Nucleo-G431RB (STM32G431RBT6)

First project on the **nucleo-g431** board: blinks the onboard user LED,
checks the USER button, and samples the ADC1 internal channels, printing
over the ST-Link virtual COM port.

## What it does

- **LD2 (PA5, green)** toggles every 250 ms (high-active)
- Holding the **USER button B1 (PC13)** lights LD2 (bring-up check)
- Every ~1 s samples the ADC1 internal channels and prints:
  chip temperature (factory-calibration interpolated), VREFINT-derived
  supply voltage and VBAT
- Banner with SYSCLK at boot

## Board facts

- STM32G431RBT6 @ **170 MHz from the 16 MHz HSI** (PLL M=4/N=85/R=2,
  regulator Range 1 BOOST; the Nucleo-64 has no HSE crystal fitted)
- LED: **LD2 = PA5** (green, high-active)
- USER button B1 = PC13 (pressed = high)
- Console: **USART2 PA2 (TX) / PA3 (RX)** - ST-LINK VCP, 115200 8-N-1
- Debug: onboard ST-Link over SWD (probe-rs auto-detects)

## Build / flash

```bash
cd app/blink_hello
bash build.sh          # == mkdir -p build && cd build && cmake -G Ninja .. && ninja
ninja flash            # probe-rs download + reset over ST-Link SWD
```

## Files

- `src/main.c` - LED blink + button check + ADC report loop
- `src/adc_internal.c/.h` - ADC1 internal-channel sampler (ch16 temp,
  ch18 VREFINT, ch17 VBAT/3; factory-calibrated at VDDA = 3.0 V)
