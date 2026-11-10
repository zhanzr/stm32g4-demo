/**
  * @file    uart_printf.c
  * @brief   USART2 printf implementation for the Nucleo-G431RB board.
  *
  * USART2 is on APB1 (170 MHz with the 170 MHz clock tree) and is wired to
  * the ST-Link's virtual COM port via PA2 (TX) and PA3 (RX), AF7.
  * Output is 115200 8-N-1, blocking (polled) so nothing is dropped.
  */

#include "uart_printf.h"
#include "stm32g4xx_hal.h"

#include <stdio.h>
#include <stdarg.h>

static UART_HandleTypeDef huart2;

/* ------------------------------------------------------------------------ */
/* printf() replacement for armclang builds (see cmake/printf_rename.h).
 * armclang would otherwise turn printf into ARMCLIB's __2printf ABI, which
 * cannot be linked against newlib. vprintf() is not specialized by armclang,
 * so this thin wrapper keeps the standard printf() working over the UART. */
int bench_printf(const char *fmt, ...)
{
    va_list args;
    int r;

    va_start(args, fmt);
    r = vprintf(fmt, args);
    va_end(args);
    return r;
}

/* ------------------------------------------------------------------------ */
void UART_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA2 = USART2_TX, PA3 = USART2_RX (AF7) - ST-LINK VCP wiring. */
    GPIO_InitStruct.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);   /* PA2/PA3 live on GPIOA */

    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200U;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

/* ------------------------------------------------------------------------ */
int UART_PutChar(int ch)
{
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&huart2, &c, 1U, 1000U);
    return ch;
}
