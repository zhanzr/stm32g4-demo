/*
  interface.c - low-level ILI9488 bus primitives (nucleo-g431 port).

  One wiring, two drive methods selected at runtime:
    SOFT: PB13/PB15 as plain GPIO outputs, bit-banged (DC pin selects
          command vs data).
    HW:   PB13/PB15 re-muxed to SPI2 AF5 (SCK/MOSI), master, 8-bit, mode 0
          (CPOL=0, CPHA=1EDGE), MSB first, NSS soft. SPI2 kernel clock =
          PCLK1 = 170 MHz on the G431 board tree: prescaler /4 =
          42.5 MHz SCK (RM0440 SPI clock tree; prescaler /2 = 85 MHz is
          an overclock experiment knob - the nucleo-f413 twin of this
          demo verified 50 MHz on the same panel family).

  Raster data in HW mode is rendered into one of two 4 KB buffers while
  DMA1 Channel 1 (DMAMUX request 13 = SPI2_TX) streams the other one at
  full wire speed, so byte generation and the wire overlap. Commands and
  short tails stay on the polled path. Solid-color fills skip byte
  generation entirely (color pattern -> repeated DMA bursts).

    SCL = PB13, MOSI = PB15, DC = PB2, RES = PB1, CS = PB14 (GPIO).
    No MISO in this wiring: 4-wire SPI, reads not supported.
*/

#include <stdint.h>
#include "lcd.h"
#include <string.h>
#include "stm32g4xx_hal.h"

/* Bytes of pixel data accumulated before one SPI2 DMA burst (HW only). */
#define SPI_TX_BUF_SIZE 4096U

/* Shortest transfer worth a DMA burst; below it the polled path wins. */
#define SPI_DMA_MIN_BYTES 32U

/* Repeating RGB666 color pattern used by solid fills (1024 pixels). */
#define SPI_PAT_BUF_SIZE 3072U

/* SPI2 baud prescaler: SPI2 kernel clock = PCLK1 = 170 MHz on the G431
 * board clock tree; /4 = 42.5 MHz SCK is the RM0440-documented default.
 * /2 = 85 MHz is an overclock experiment knob (LCD_SPI2_MBR overrides).
 * Values are SPI_BAUDRATEPRESCALER_x (0 = /2 .. 7 = /256). */
#ifndef LCD_SPI2_PRESC
#define LCD_SPI2_PRESC SPI_BAUDRATEPRESCALER_4
#endif

/* Two raster buffers: the CPU fills one while DMA streams the other. */
static uint8_t              s_tx_buf[2][SPI_TX_BUF_SIZE];
static uint8_t              s_pat_buf[SPI_PAT_BUF_SIZE];
static uint8_t              s_tx_cur;      /* buffer being filled         */
static uint16_t             s_tx_len;      /* bytes filled in it          */
static uint8_t              s_dma_busy;    /* a burst is in flight        */
static SPI_HandleTypeDef    s_hspi;
static uint8_t              s_spi_ready;
static uint8_t              s_bus_hw;      /* 0 = soft (GPIO), 1 = HW SPI2 */

/* Soft-bus sweep knobs (the bit rate is critical for this module). */
static uint8_t s_soft_pace = 0U;   /* volatile-loop iterations per half-bit */
static uint8_t s_soft_slew_high = 1U;

static void SendDataSPI(uint8_t dat);   /* soft bit-bang, defined below */
static void dma_tx_wait(void);          /* TC-based burst completion wait */

void CS_SET(void)
{
    LCD_CS_SET;
}
void CS_CLR(void)
{
    LCD_CS_CLR;
}

/* Configure + init SPI2 (once). GPIO re-mux happens in LCD_UseHwBus. */
static void spi_hw_init(void)
{
    if (s_spi_ready != 0U)
    {
        return;
    }
    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();        /* SPI2_TX raster bursts */
    __HAL_RCC_DMAMUX1_CLK_ENABLE();

    /* Route DMAMUX channel 0 to SPI2_TX (request 13). NOTE the 0-based
     * DMAMUX numbering: mux channel 0 feeds DMA1_Channel1, mux channel 1
     * feeds DMA1_Channel2, ... (DMA channels are 1-based). */
    DMAMUX1_Channel0->CCR = DMA_REQUEST_SPI2_TX;

    s_hspi.Instance               = SPI2;
    s_hspi.Init.Mode              = SPI_MODE_MASTER;
    s_hspi.Init.Direction         = SPI_DIRECTION_2LINES;     /* full duplex */
    s_hspi.Init.DataSize          = SPI_DATASIZE_8BIT;
    s_hspi.Init.CLKPolarity       = SPI_POLARITY_LOW;         /* mode 0 */
    s_hspi.Init.CLKPhase          = SPI_PHASE_1EDGE;
    s_hspi.Init.NSS               = SPI_NSS_SOFT;
    s_hspi.Init.BaudRatePrescaler = LCD_SPI2_PRESC;
    s_hspi.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    s_hspi.Init.TIMode            = SPI_TIMODE_DISABLE;
    s_hspi.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    s_hspi.Init.CRCPolynomial     = 7U;
    (void)HAL_SPI_Init(&s_hspi);

    s_spi_ready = 1U;
}

/* Ensure the SPI is enabled: HAL transfers disable SPE when they close,
 * and the register-level paths below rely on SPE being set (with SPE=0
 * the TX buffer never drains and neither TXE nor DMA requests follow). */
static void spi_hw_enable(void)
{
    if ((SPI2->CR1 & SPI_CR1_SPE) == 0U)
    {
        SPI2->CR1 |= SPI_CR1_SPE;
    }
}

/* ---------------- SPI2_TX DMA (DMA1 Channel 1, DMAMUX req 13) -------- */

/* Arm and start one burst. Waits for a previous burst to finish first,
 * so the buffer it reads is never rewritten under it. */
static void dma_tx_start(const uint8_t *buf, uint32_t len)
{
    dma_tx_wait();               /* finish any in-flight burst first */

    DMA1->IFCR = DMA_ISR_TCIF1;            /* clear transfer-complete */

    DMA1_Channel1->CPAR  = (uint32_t)&SPI2->DR;
    DMA1_Channel1->CMAR  = (uint32_t)buf;
    DMA1_Channel1->CNDTR = len;
    DMA1_Channel1->CCR   = DMA_CCR_MINC                    /* mem++        */
                         | DMA_CCR_DIR                     /* mem -> per   */
                         | DMA_CCR_PL_1;                   /* high prio    */

    SPI2->CR2 |= SPI_CR2_TXDMAEN;
    DMA1_Channel1->CCR |= DMA_CCR_EN;
    s_dma_busy = 1U;
}

/* Wait for the in-flight burst, then let the shifter drain. On the G4 DMA
 * the EN bit STAYS SET after the transfer completes (unlike F4 streams),
 * so completion is polled via the channel TC flag, not via EN. The RX side
 * is left alone here: full-duplex echoes overrun (harmlessly) during a
 * burst and are cleared once per burst group by SPI_HW_Flush. */
static void dma_tx_wait(void)
{
    if (s_dma_busy == 0U)
    {
        return;
    }
    while ((DMA1->ISR & DMA_ISR_TCIF1) == 0U) { }          /* burst complete */
    DMA1->IFCR = DMA_ISR_TCIF1;                            /* clear it */
    DMA1_Channel1->CCR &= ~DMA_CCR_EN;                     /* disarm (EN lingers) */
    while ((SPI2->SR & SPI_SR_BSY) != 0U) { }              /* shifter drain */
    SPI2->CR2 &= ~SPI_CR2_TXDMAEN;
    s_dma_busy = 0U;
}

/* Push a full buffer: start the burst, then flip to the other buffer so
 * the caller keeps filling while the wire is busy. */
static void spi_push_buf(void)
{
    uint8_t *p = s_tx_buf[s_tx_cur];
    uint16_t n = s_tx_len;

    s_tx_len = 0U;
    s_tx_cur ^= (uint8_t)1U;
    spi_hw_enable();
    dma_tx_start(p, n);
}

/* Small/blocking transmit (commands, tails): polled per-byte TXE writes. */
static void spi_hw_tx(uint8_t *buf, uint16_t len)
{
    uint16_t i;

    if (len == 0U)
    {
        return;
    }
    if (s_bus_hw == 0U)
    {
        for (i = 0U; i < len; i++)
        {
            SendDataSPI(buf[i]);
        }
        return;
    }

    if (len >= SPI_DMA_MIN_BYTES)
    {
        spi_hw_enable();
        dma_tx_start(buf, len);
        dma_tx_wait();
    }
    else
    {
        spi_hw_enable();
        for (i = 0U; i < len; i++)
        {
            while ((SPI2->SR & SPI_SR_TXE) == 0U) { }
            *((__IO uint8_t *)&SPI2->DR) = buf[i];
        }
        while ((SPI2->SR & SPI_SR_BSY) != 0U) { }
    }

    while ((SPI2->SR & SPI_SR_RXNE) != 0U) { (void)SPI2->DR; }
    if ((SPI2->SR & SPI_SR_OVR) != 0U)
    {
        (void)SPI2->DR;
        (void)SPI2->SR;
    }
}

void SPI_HW_Flush(void)
{
    if (s_tx_len != 0U)
    {
        if (s_tx_len >= SPI_DMA_MIN_BYTES)
        {
            spi_push_buf();
        }
        else
        {
            uint8_t *p = s_tx_buf[s_tx_cur];
            uint16_t n = s_tx_len;

            dma_tx_wait();
            s_tx_len = 0U;
            spi_hw_tx(p, n);
        }
    }

    dma_tx_wait();

    while ((SPI2->SR & SPI_SR_RXNE) != 0U) { (void)SPI2->DR; }
    if ((SPI2->SR & SPI_SR_OVR) != 0U)
    {
        (void)SPI2->DR;
        (void)SPI2->SR;
    }
}

/* ---------------- bus selection ------------------------------------- */

void LCD_UseSoftBus(void)
{
    GPIO_InitTypeDef g;

    SPI_HW_Flush();
    s_bus_hw = 0U;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Speed = s_soft_slew_high ? GPIO_SPEED_FREQ_HIGH : GPIO_SPEED_FREQ_LOW;
    g.Pull  = GPIO_NOPULL;
    g.Pin   = LCD_SCL_Pin | LCD_SDA_Pin;
    HAL_GPIO_Init(LCD_GPIO_PortSCL, &g);
    /* idle SCL/SDA high; soft is TX-only */
    LCD_SPI_SCL_SET;
    LCD_SPI_SDA_SET;
}

void LCD_UseHwBus(void)
{
    GPIO_InitTypeDef g;

    s_bus_hw = 1U;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    g.Mode      = GPIO_MODE_AF_PP;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Pull      = GPIO_NOPULL;
    g.Alternate = GPIO_AF5_SPI2;         /* PB13 = SCK, PB15 = MOSI */
    g.Pin       = LCD_SCL_Pin | LCD_SDA_Pin;
    HAL_GPIO_Init(LCD_GPIO_PortSCL, &g);

    spi_hw_init();
}

uint8_t LCD_BusIsHw(void)
{
    return s_bus_hw;
}

/* Runtime SCK switch (speed sweep): write the BR field with SPE off.
 * `presc` is the SPI_BAUDRATEPRESCALER_x value (0=/2 .. 7=/256). */
void LCD_HwSetPrescaler(uint8_t presc)
{
    SPI_HW_Flush();
    __HAL_SPI_DISABLE(&s_hspi);
    SPI2->CR1 = (SPI2->CR1 & ~SPI_CR1_BR) | (presc & SPI_CR1_BR);
    __HAL_SPI_ENABLE(&s_hspi);
    s_hspi.Init.BaudRatePrescaler = presc & SPI_CR1_BR;
}

unsigned long LCD_HwSpiKHz(void)
{
    uint32_t div = 2U << ((s_hspi.Init.BaudRatePrescaler & SPI_CR1_BR) >>
                          SPI_CR1_BR_Pos);
    return (unsigned long)(HAL_RCC_GetPCLK1Freq() / div / 1000U);
}

/* ---------------- SOFT path: bit-bang PB13/PB15 ---------------------- */

void LCD_SoftSetPace(uint8_t iterations)
{
    s_soft_pace = iterations;
}

void LCD_SoftSetSlew(uint8_t high)
{
    GPIO_InitTypeDef g;

    s_soft_slew_high = high;
    __HAL_RCC_GPIOB_CLK_ENABLE();
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = high ? GPIO_SPEED_FREQ_HIGH : GPIO_SPEED_FREQ_LOW;
    g.Pin   = LCD_SCL_Pin | LCD_SDA_Pin;
    HAL_GPIO_Init(LCD_GPIO_PortSCL, &g);
}

static void SendDataSPI(uint8_t dat)
{
    /* Direct BSRR writes with a pace knob. SCL = PB13, SDA = PB15. */
    for (int i = 0; i < 8; i++)
    {
        GPIOB->BSRR = (1U << (13 + 16));      /* SCL low (BSRR reset half) */
        for (volatile int d = 0; d < s_soft_pace; d++) { }

        if ((dat & 0x80U) != 0U)
        {
            GPIOB->BSRR = (1U << 15);         /* SDA high */
        }
        else
        {
            GPIOB->BSRR = (1U << (15 + 16));  /* SDA low (BSRR reset half) */
        }
        for (volatile int d = 0; d < s_soft_pace; d++) { }

        GPIOB->BSRR = (1U << 13);             /* SCL high - panel samples */
        dat <<= 1;
    }
}

/* ---------------- byte-level transfers ------------------------------- */

/* Send one byte immediately (commands): bus-dependent, no buffering. */
static void spi_send_now(uint8_t dat)
{
    if (s_bus_hw != 0U)
    {
        uint8_t rx;
        spi_hw_enable();
        while ((SPI2->SR & SPI_SR_TXE) == 0U) { }
        *((__IO uint8_t *)&SPI2->DR) = dat;
        while ((SPI2->SR & SPI_SR_BSY) != 0U) { }
        while ((SPI2->SR & SPI_SR_RXNE) != 0U) { rx = (uint8_t)SPI2->DR; }
        (void)rx;
        if ((SPI2->SR & SPI_SR_OVR) != 0U)
        {
            (void)SPI2->DR;
            (void)SPI2->SR;
        }
    }
    else
    {
        SendDataSPI(dat);
    }
}

/* Queue/send one data byte (DC already high). */
static void spi_put(uint8_t dat)
{
    if (s_bus_hw == 0U)
    {
        SendDataSPI(dat);
        return;
    }
    s_tx_buf[s_tx_cur][s_tx_len++] = dat;
    if (s_tx_len >= SPI_TX_BUF_SIZE)
    {
        spi_push_buf();
    }
}

/* Write a register/command: DC low. */
void WriteComm(uint16_t data)
{
    SPI_HW_Flush();
    LCD_CS_CLR;
    LCD_RS_CLR;
    spi_send_now((uint8_t)data);
    LCD_CS_SET;
}

/* Write a data byte: DC high. */
void WriteData(uint16_t data)
{
    SPI_HW_Flush();
    LCD_CS_CLR;
    LCD_RS_SET;
    spi_send_now((uint8_t)data);
    LCD_CS_SET;
}

/* Stream one RGB565 pixel color as two 8-bit writes (DC high persists). */
void SendData(uint32_t color)
{
    LCD_CS_CLR;
    LCD_RS_SET;
    spi_put((uint8_t)(color >> 8));
    spi_put((uint8_t)color);
    SPI_HW_Flush();
    LCD_CS_SET;
}

/* Fast raw 8-bit data byte: no CS/DC toggling (caller manages both). */
void LCD_WriteDataFast(uint8_t data)
{
    spi_put(data);
}

/* Push one RGB565 pixel as the panel's 18-bit RGB666 frame (3 bytes).
 * One call per pixel instead of three keeps raster loops cheap; the
 * buffer is checked once per pixel so a flush never splits a frame. */
void LCD_WritePixelRGB565(uint16_t c565)
{
    uint8_t r = (uint8_t)(((c565 >> 8) & 0xF8U) | ((c565 >> 13) & 0x07U));
    uint8_t g = (uint8_t)(((c565 >> 3) & 0xFCU) | ((c565 >> 9) & 0x03U));
    uint8_t b = (uint8_t)((c565 << 3) & 0xF8U);

    if (s_bus_hw == 0U)
    {
        SendDataSPI(r);
        SendDataSPI(g);
        SendDataSPI(b);
        return;
    }

    if (s_tx_len > (SPI_TX_BUF_SIZE - 3U))
    {
        spi_push_buf();
    }

    {
        uint8_t *p = &s_tx_buf[s_tx_cur][s_tx_len];
        p[0] = r;
        p[1] = g;
        p[2] = b;
        s_tx_len = (uint16_t)(s_tx_len + 3U);
    }
}

/* Stream `npix` pixels of one color: repeated DMA bursts of a prepared
 * color pattern on the HW bus, plain bit-bang on the soft bus. */
void LCD_StreamColor565(uint16_t c565, uint32_t npix)
{
    uint8_t  r = (uint8_t)(((c565 >> 8) & 0xF8U) | ((c565 >> 13) & 0x07U));
    uint8_t  g = (uint8_t)(((c565 >> 3) & 0xFCU) | ((c565 >> 9) & 0x03U));
    uint8_t  b = (uint8_t)((c565 << 3) & 0xF8U);
    uint32_t total;
    uint32_t len;

    if (npix == 0U)
    {
        return;
    }

    if (s_bus_hw == 0U)
    {
        while (npix-- != 0U)
        {
            SendDataSPI(r);
            SendDataSPI(g);
            SendDataSPI(b);
        }
        return;
    }

    total = npix * 3U;
    len   = (total < (uint32_t)SPI_PAT_BUF_SIZE) ? total
                                                 : (uint32_t)SPI_PAT_BUF_SIZE;
    len  -= len % 3U;

    for (uint32_t i = 0U; i < len; i += 3U)
    {
        s_pat_buf[i]      = r;
        s_pat_buf[i + 1U] = g;
        s_pat_buf[i + 2U] = b;
    }

    spi_hw_enable();
    while (total != 0U)
    {
        uint32_t n = (total < len) ? total : len;

        dma_tx_start(s_pat_buf, n);
        dma_tx_wait();
        total -= n;
    }

    while ((SPI2->SR & SPI_SR_RXNE) != 0U) { (void)SPI2->DR; }
    if ((SPI2->SR & SPI_SR_OVR) != 0U)
    {
        (void)SPI2->DR;
        (void)SPI2->SR;
    }
}

/* Begin/end a data-gram burst for raster fills: DC high, CS held low. */
void LCD_BeginData(void)
{
    SPI_HW_Flush();       /* never mix a command byte into a data burst */
    LCD_CS_CLR;
    LCD_RS_SET;
}
void LCD_EndData(void)
{
    SPI_HW_Flush();
    LCD_CS_SET;
}
