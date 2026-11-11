# ips395-st7796-captouch - ST7796S 4.0" 320x480 IPS + FT6336U on nucleo-g431

Port of the **nucleo-f413** twin project: drives the **IPS395-ST7796**
4.0" **320x480** IPS module (**ST7796S** display + **FT6336U**
capacitive touch) on the **nucleo-g431** board (STM32G431RBT6 @ 170 MHz)
over a 4-wire SPI bus, with two interchangeable drive methods compared
back to back in one demo loop: bit-banged GPIO ("soft SPI") and the
**SPI2** peripheral with DMA raster bursts (**42.5 MHz**, PCLK1/4).

**Verified on the connected panel + touch: FT6336U answers on I2C1 at
400 kHz, and the demo loop runs on both buses.**

## Hardware connection plan

Display wiring is identical to `sx395hvga40p-01-v01` (same port set):

| LCD pin | Nucleo-G431RB pin | G431 capability |
| ------- | ----------------- | --------------- |
| SCL/SCK | **PB13** | SPI2_SCK (AF5), or bit-bang GPIO |
| SDA/MOSI | **PB15** | SPI2_MOSI (AF5) |
| CS | **PB14** | plain GPIO (software CS - required by the 4-line protocol) |
| DC/RS | **PB2** | plain GPIO |
| RST | **PB1** | plain GPIO |
| BL | **PB11** | TIM2_CH4 (AF1) - 1 kHz PWM |

Touch connections:

| Touch pin | Nucleo-G431RB pin | G431 capability |
| --------- | ----------------- | --------------- |
| T_SCL | **PB8** | I2C1_SCL (AF4), open drain |
| T_SDA | **PB9** | I2C1_SDA (AF4), open drain |
| T_INT | **PC6** | GPIO input, pull-up (open-drain, active low) |
| T_RST | **PC8** | GPIO output, idle high |

Feasibility notes:

- PB8/PB9 are exactly the I2C1 AF4 pair on this family - the touch gets
  a hardware I2C master. I2C1 kernel clock = PCLK1 = 170 MHz (no
  selector on the G431); the HAL I2C `Timing` value in `src/touch.c`
  (`0x70B00F20`) is computed for 170 MHz / 400 kHz fast mode with
  margin for the module's 10K pull-ups.
- PC6/PC8 are plain GPIO on the LQFP64; PC6 sits on EXTI line 6 (unused
  here - the demo polls every ~5 ms and only uses INT as a hint).
- No conflicts: display PB13/14/15 + PB1/2/11, console USART2 PA2/PA3,
  LED LD2 PA5, touch PB8/PB9 + PC6/PC8 all disjoint.
- The module carries 10K pull-ups on SDA/SCL; keep the leads short.

## Controller notes (vendor init, from the f413 twin)

- **ST7796S**, 320x480 IPS: sleep out + 120 ms, `F0=C3`/`F0=96`
  (Command2 unlock), MADCTL (knob, default **0x48**), COLMOD,
  `B4=01`, `B7=C6`, `E8=...`, `C1/C2/C5`, gamma `E0/E1`, `F0=3C`/
  `F0=69` (BK1), `21` (inversion on) + 120 ms, `29` (display on).
- **RGB565 in 4-line serial** (COLMOD `0x3A = 0x55`): two bytes per
  pixel, no per-pixel color conversion - a full frame is 307,200 bytes
  (~1.5x cheaper than the ILI9488's 18-bit frames).
- **MADCTL (`0x36`) is the mirror knob** (default 0x48 = MX + RGB,
  vendor portrait). Override at build time: `bash build.sh
  -DLCD_MADCTL=0x88` (re-run cmake). Clear bit 3 if red/blue swap.
- **FT6336U**: slave address 0x38 (7-bit), 400 kHz fast mode. Init
  reads the 16-byte info block at 0xA0 (printed on the console), sets
  `ID_G_MODE (0xA4) = 0` (interrupt trigger). Each scan reads
  `TD_STATUS (0x02)` + the 6-byte point block at `0x03`; the event flag
  in XH bits 7:6 is honoured (press/contact accepted, release dropped).
  A NACK/stuck bus is recovered by an I2C software reset (SWRST) +
  re-init. No NVM: mapping knobs are build macros (`TP_FLIP_X`,
  `TP_FLIP_Y`, `TP_SWAP_XY`, `TP_RAW_W/H`).

## DMA port gotchas (inherited from sx395hvga40p-01-v01, verified there)

- **DMAMUX channel numbering is 0-based**: `DMA1_Channel1` is routed by
  `DMAMUX1_Channel0` (request 13 = SPI2_TX).
- **G4 DMA `CCR.EN` stays set after TC** - completion is polled via the
  channel TC flag, not via EN.
- Full-duplex overrun during bursts is harmless (TX keeps running);
  flags are drained at flush points.

## Demo loop

Same shape as the f413 twin and the sx395 port: each bus (soft, then HW)
runs banner + TEST_STAND (timed solids) + info (normal + inverted,
including the FT6336 status) + HSV gradient + **touch test** (five
targets, paint trail, live x/y readout, 15 s) + LED test, with a live
FPS counter. Touch events (press/release with mapped + raw coordinates)
print on the console.

## Build / flash

```bash
cd app/ips395-st7796-captouch
bash build.sh          # == mkdir -p build && cd build && cmake -G Ninja .. && ninja
ninja flash            # probe-rs download + reset over ST-Link SWD
```

Console: USART2 PA2/PA3, 115200 8-N-1 (ST-LINK VCP). Capture helper at
the repo root: `powershell -File capture_console.ps1 -Seconds 14 -Out <file>`.

## Files

- `src/main.c` - pattern set on both buses: banner, info (normal +
  inverted), TEST_STAND (timed solids), gradient, touch test, LED test
- `src/interface.c` / `interface.h` - soft bit-bang + HW SPI2 dual-bus
  primitives, double-buffered SPI2_TX DMA bursts, 16-bit color streamer
- `src/lcd.c` / `lcd.h` - ST7796S init + 320x480 geometry + drawing API
  (RGB565 pixel writer, single-pass page renderer, `LCD_MADCTL` knob)
- `src/touch.c` / `touch.h` - FT6336U on I2C1 (PB8/PB9, RST PC8,
  INT PC6): reset/probe, interrupt-mode setup, point read + mapping
- `src/backlight.c` / `backlight.h` - PB11/TIM2_CH4 PWM (+ duty getter)
- `src/lcd/lcd_fonts.c` / `lcd_fonts.h` - ASCII 6x12 font
- `src/lcd/lcd_font_1608.c` / `lcd_font_1608.h` - ASCII 8x16 banner font
- `src/blockwrite/blockwrite.h` - pixel-window helper
