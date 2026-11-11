/*
  backlight.c - LCD backlight (BL = PB11) PWM via TIM2_CH4.

  PB11 is TIM2_CH4 (AF1). TIM2 is a general-purpose 32-bit timer on APB1;
  with the G431 170 MHz clock tree the timer clock is 170 MHz.
  PSC/ARR of 169/999 gives a 1 kHz PWM; CCR4 = 0..999 scales 0..100%.

  Plain register setup (no HAL TIM module needed for this small demo).
*/

#include "backlight.h"
#include "lcd.h"

void Backlight_Init(void)
{
    GPIO_InitTypeDef g;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();

    /* PB11 = TIM2_CH4, alternate function push-pull. */
    g.Pin   = LCD_BL_Pin;
    g.Mode  = GPIO_MODE_AF_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(LCD_GPIO_PortBL, &g);

    /* 1 kHz PWM: timer clock 170 MHz / (PSC+1) / (ARR+1). */
    TIM2->CR1  = 0;
    TIM2->PSC  = 169;
    TIM2->ARR  = 999;
    TIM2->CCR4 = 150;                  /* 15% at init (main re-sets it) */
    TIM2->CCMR2 |= (TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2)  /* PWM mode 1 */
                 | TIM_CCMR2_OC4PE;    /* preload enabled */
    TIM2->CCER  |= TIM_CCER_CC4E;      /* CH4 output enable */
    TIM2->CR1   |= TIM_CR1_CEN;        /* start the counter */
}

void Backlight_SetDuty(uint16_t percent)
{
    if (percent > 100U)
    {
        percent = 100U;
    }
    TIM2->CCR4 = (uint32_t)percent * (TIM2->ARR + 1U) / 100U;
}

uint16_t Backlight_GetDuty(void)
{
    return (uint16_t)((TIM2->CCR4 * 100U) / (TIM2->ARR + 1U));
}
