/**
  * @file    cordic_hw.c
  * @brief   Hardware CORDIC IP (sine) wrapper.
  *
  * Configuration follows ST's NUCLEO-G431RB CORDIC_Sin_DMA example:
  * function = SINE, precision = 6 cycles ("max precision for q1.31 sine"),
  * 1 arg in (angle, q1.31, 2^31 = pi), 1 result out (sine, q1.31).
  */

#include "cordic_hw.h"
#include "board.h"
#include "stm32g4xx_hal.h"

static CORDIC_HandleTypeDef hcordic;

/* ------------------------------------------------------------------------ */
void CORDIC_HW_Init(void)
{
    CORDIC_ConfigTypeDef cfg = {0};

    __HAL_RCC_CORDIC_CLK_ENABLE();

    hcordic.Instance = CORDIC;
    if (HAL_CORDIC_Init(&hcordic) != HAL_OK)
    {
        Error_Handler();
    }

    cfg.Function = CORDIC_FUNCTION_SINE;        /* sine function */
    cfg.Precision = CORDIC_PRECISION_6CYCLES;   /* max precision for q1.31 sine */
    cfg.Scale = CORDIC_SCALE_0;                 /* no scale */
    cfg.NbWrite = CORDIC_NBWRITE_1;             /* one input: angle (modulus = 1 after reset) */
    cfg.NbRead = CORDIC_NBREAD_1;               /* one output: sine */
    cfg.InSize = CORDIC_INSIZE_32BITS;          /* q1.31 input */
    cfg.OutSize = CORDIC_OUTSIZE_32BITS;        /* q1.31 output */
    if (HAL_CORDIC_Configure(&hcordic, &cfg) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ------------------------------------------------------------------------ */
void CORDIC_HW_Sin_List(const int32_t *in_q31, int32_t *out_q31, uint32_t len)
{
    if (HAL_CORDIC_Calculate(&hcordic, in_q31, out_q31, len, 1000U) != HAL_OK)
    {
        Error_Handler();
    }
}
