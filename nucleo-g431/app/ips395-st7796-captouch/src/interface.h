/*
  interface.h - low-level ST7796S bus primitives (nucleo-g431 port).

  Standard 4-wire serial: CS frames each transfer, DC selects command vs
  data, the panel latches SDA on the rising edge. Two drive methods,
  selected at runtime:

    SOFT: PB13/PB15 bit-banged (idle-high SCL).
    HW  : SPI2 AF5 (PB13 = SCK, PB15 = MOSI), mode 0, 8-bit, MSB first,
          NSS soft. SPI2 kernel clock = PCLK1 = 170 MHz; the default
          prescaler /4 gives a 42.5 MHz SCK (RM0440 SPI clock tree;
          /2 = 85 MHz is available as an overclock experiment - see the
          README). HW raster data is double-buffered and streamed by
          DMA1 Channel 1 (DMAMUX request 13 = SPI2_TX) at wire speed;
          solid fills use a repeating color pattern, so they are
          wire-limited too.

    SCL = PB13, SDA = PB15 (soft GPIO or SPI2 AF5)
    DC  = PB2 (data/command, always GPIO)
    RES = PB1 (reset, always GPIO)
    CS  = PB14 (chip select, always GPIO)
    BL  = PB11 (TIM2_CH4 backlight PWM)

    No MISO in this wiring (4-wire SPI: panel reads are not supported).
*/

#ifndef __INTERFACE_H
#define __INTERFACE_H

#include <stdint.h>

void CS_SET(void);
void CS_CLR(void);
void WriteComm(uint16_t data);
void WriteData(uint16_t data);
void SendData(uint32_t color);
void LCD_WriteDataFast(uint8_t data);   /* raw byte, caller manages CS/DC */
void LCD_BeginData(void);                /* DC high, CS low, for raster bursts */
void LCD_EndData(void);                  /* CS high after a burst */

/* ---- bus selection (SOFT bit-bang vs HW SPI2) ---- */
void    LCD_UseSoftBus(void);   /* re-mux PB13/PB15 to GPIO, idle high      */
void    LCD_UseHwBus(void);     /* re-mux PB13/PB15 to SPI2 AF5 + init      */
uint8_t LCD_BusIsHw(void);      /* 1 while the HW SPI2 bus is selected      */
unsigned long LCD_HwSpiKHz(void); /* active SPI2 baud in kHz (info page)    */
void    LCD_HwSetPrescaler(uint8_t presc); /* runtime SCK switch (sweep)   */
void    LCD_SoftSetPace(uint8_t iterations); /* per-bit delay (sweep)       */
void    LCD_SoftSetSlew(uint8_t high);   /* GPIO slew LOW/HIGH (sweep)      */
void    SPI_HW_Flush(void);     /* drain the HW TX buffer (blocking)        */

/* One RGB565 pixel (2 bytes, high byte first - COLMOD 0x55). */
void    LCD_WritePixel16(uint16_t c565);

/* npix pixels of one RGB565 color: DMA color-pattern bursts on the HW
 * bus (wire-limited), bit-bang on the soft bus. DC high, CS low. */
void    LCD_StreamColor16(uint16_t c565, uint32_t npix);

#endif /* __INTERFACE_H */
