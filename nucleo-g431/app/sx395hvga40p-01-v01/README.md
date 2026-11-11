# sx395hvga40p-01-v01 - ILI9488 3.5" 320x480 LCD on nucleo-g431

Port of the **nucleo-f413** twin project: drives the **SX395HVGA40P-01-V01**
3.5" **320x480** module (**ILI9488** controller) on the **nucleo-g431**
board (STM32G431RBT6 @ 170 MHz) over a 4-wire SPI bus, with two
interchangeable drive methods compared back to back in one demo loop:
bit-banged GPIO ("soft SPI", ~700 ms per full-screen fill) and the
**SPI2** peripheral with DMA raster bursts (**89 ms per fill** = wire
speed at 42.5 MHz). **Verified on the connected panel.**

## Hardware connection plan - feasibility (investigated)

| LCD pin | Nucleo-G431RB pin | G431 capability | Verdict |
| ------- | ----------------- | --------------- | ------- |
| SCL/SCK | **PB13** | SPI2_SCK (AF5), or GPIO output for bit-bang | **OK** |
| SDA/MOSI | **PB15** | SPI2_MOSI (AF5), or GPIO output | **OK** |
| CS | **PB14** | plain GPIO (PB14 is also SPI2_MISO AF5 - unused here) | **OK** - software CS is required anyway: the ILI9488 needs CS to frame every command/data transfer, which hardware NSS cannot do |
| DC/RS | **PB2** | plain GPIO output | **OK** |
| RST | **PB1** | plain GPIO output | **OK** |
| BL | **PB11** | TIM2_CH4 (AF1) - 32-bit timer, APB1, timer clock 170 MHz | **OK** - 1 kHz PWM via PSC=169/ARR=999 |

Bus-level facts verified for this chip:

- **SPI2 kernel clock = PCLK1 = 170 MHz fixed** (the G431 RCC has no
  selector for it). Prescaler /4 = **42.5 MHz SCK** is the default
  (close to the 50 MHz the nucleo-f413 twin verified on the same panel
  family); `/2 = 85 MHz` is available as an overclock experiment via
  `LCD_SPI2_PRESC` / `LCD_HwSetPrescaler()` - re-verify on hardware.
- **SPI2_TX DMA**: the G4 routes DMA requests through **DMAMUX** -
  SPI2_TX is DMAMUX request **13**, driven on **DMA1 Channel 1**
  (any channel works). The nucleo-f413 version used the fixed
  DMA2 Stream5/channel-3 mapping; the F4 register-level burst pump ports
  1:1 apart from DMAMUX (see `src/interface.c`, `dma_tx_start()`).
- **The G431 SPI is the same classic IP as the F4's** (CR1/SR/DR
  registers, TXE/BSY/RXNE/OVR flags), so the polled byte pump and DMA
  handshake code port unchanged - only the instance, clock and DMA
  routing differ.
- No MISO wire in this plan: 4-wire SPI reads are not supported anyway
  (the f413 twin verified every read returns 0x00), so the read-back
  helper was dropped from the port.
- Resources: 58.5 KB text (fits 128 KB flash), 16.3 KB bss - the two 4 KB
  DMA raster buffers + 3 KB color-pattern buffer + gradient row fit the
  32 KB SRAM with ~15 KB headroom.
- Pin conflicts: none. LD2 (PA5) stays free and is used for the demo's
  LED test; PB13/14/15 and PB1/2/11 have no other users on this board.

## DMA port gotchas (found on hardware, worth remembering)

- **DMAMUX channel numbering is 0-based, DMA channel numbering is
  1-based**: `DMA1_Channel1` is routed by **`DMAMUX1_Channel0`**
  (`DMAMUX1_Channel1` feeds `DMA1_Channel2`!). Pairing mux ch1 with DMA
  ch1 silently routes the request to a different channel - the burst
  then never completes.
- **The G4 DMA `CCR.EN` stays set after the transfer completes**
  (unlike F4 streams where EN auto-clears at TC). Completion must be
  polled via the channel TC flag (`DMA1->ISR & DMA_ISR_TCIF1`, then
  clear it in `DMA->IFCR`), not by waiting for EN to clear. This applies
  to *both* wait points: after arming a burst, AND before re-arming the
  next one when double-buffering (the second miss only showed up once
  the per-pixel buffered path of the info page ran, since the solid
  fills never double-buffer).
- Full-duplex echoes overrun the RX FIFO during bursts (OVR/FRE flags
  set) - on this SPI that is harmless: TX keeps running at wire speed.
  The flags are drained at the flush points.

## Garble fix (display off during repaint)

After `LCD_Reinit()` the panel's display is ON while its GRAM still holds
old/random content, and the slow soft-bus clear repaints progressively -
that was the "period of garble before the banner". The demo now issues
Display OFF (0x28) right after each `LCD_Reinit()` and Display ON (0x29)
once the banner is fully painted (`WriteComm(0x29)` first flushes the
queued bytes), so each pass appears as an instantly complete banner.

## Controller notes (vendor-verbatim init, from the f413 twin)

- **ILI9488** init: vendor-register unlock (0xF7 = A9 51 2C 82),
  power/timing (0xC0, 0xC1, 0xC5 VCOM, 0xB1 frame rate, 0xB4, 0xB6,
  0xB7, 0xBE, 0xE9), MADCTL **0x08**, **18-bit COLMOD (0x3A = 0x66)** -
  REQUIRED in serial mode (2-byte pixels garble everything), gamma
  tables (0xE0/0xE1), sleep out + **120 ms**, display on.
- **Geometry**: 320x480, windows at **COL_Pre = 0, ROW_Pre = 0**.
- **Reset**: RST high 50 ms -> low 200 ms -> high + 120 ms, then init.

## Demo loop

Same as the f413 twin: each bus (soft, then HW) runs the full-size
pattern set - big-font banner, TEST_STAND (timed solid fills, live FPS),
info page (normal + inverted: compiler, build, frequency, drive method,
IO map, backlight duty, UID, measured fill durations), animated HSV
gradient, LED test (board LD2).

## Files

- `src/main.c` - pattern set on both buses: banner, info (normal +
  inverted), TEST_STAND (timed solids), gradient, LED test
- `src/interface.c` / `interface.h` - soft bit-bang + HW SPI2 dual-bus
  primitives, double-buffered SPI2_TX DMA bursts (DMA1 Ch1 / DMAMUX
  request 13), color-pattern streamer
- `src/lcd.c` / `lcd.h` - ILI9488 init + 320x480 geometry + drawing API
  (RGB666 pixel writer, single-pass page renderer) + `LCD_Reinit`
- `src/backlight.c` / `backlight.h` - PB11/TIM2_CH4 PWM (+ duty getter)
- `src/lcd/lcd_fonts.c` / `lcd_fonts.h` - ASCII 6x12 font
- `src/lcd/lcd_font_1608.c` / `lcd_font_1608.h` - ASCII 8x16 banner font
- `src/blockwrite/blockwrite.h` - pixel-window helper

## Build / flash

```bash
cd app/sx395hvga40p-01-v01
bash build.sh          # == mkdir -p build && cd build && cmake -G Ninja .. && ninja
ninja flash            # probe-rs download + reset over ST-Link SWD
```

Console: USART2 PA2/PA3, 115200 8-N-1 (ST-LINK VCP) - the demo logs every
phase and the measured solid-fill durations. Note: without the panel
connected the demo still runs (writes go nowhere) and the console keeps
logging - handy as a bring-up check while wiring.
