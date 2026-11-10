/**
  * @file    adc_internal.c
  * @brief   ADC1 internal-channel sampling (temperature / VREFINT / VBAT).
  *
  * ADC1 is on APB2 (170 MHz with the 170 MHz clock tree). The ADC kernel
  * clock is PCLK2/4 = 42.5 MHz (within the 60 MHz max for G431). 640.5-cycle
  * sample time = ~15 us (the temp sensor needs a long sample, datasheet
  * min ~5 us).
  *
  * Factory calibration (system memory, 12-bit codes at VDDA = 3.0 V):
  *   TS_CAL1     0x1FFF75A8  temp-sensor code at 30 C
  *   TS_CAL2     0x1FFF75CA  temp-sensor code at 110 C
  *   VREFINT_CAL 0x1FFF75AA  VREFINT code (VREFINT_CAL_VREF = 3000 mV)
  */

#include "adc_internal.h"
#include "board.h"
#include "stm32g4xx_hal.h"

static ADC_HandleTypeDef hadc1;

/* ------------------------------------------------------------------------ */
void ADC_Internal_Init(void)
{
    __HAL_RCC_ADC12_CLK_ENABLE();

    hadc1.Instance                    = ADC1;
    hadc1.Init.ClockPrescaler         = ADC_CLOCK_SYNC_PCLK_DIV4;  /* 170/4 = 42.5 MHz */
    hadc1.Init.Resolution             = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign              = ADC_DATAALIGN_RIGHT;
    hadc1.Init.GainCompensation       = 0U;         /* gain compensation off */
    hadc1.Init.ScanConvMode           = ADC_SCAN_ENABLE;     /* scan the 3 internal channels */
    hadc1.Init.EOCSelection           = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait       = DISABLE;
    hadc1.Init.ContinuousConvMode     = DISABLE;     /* one scan per HAL_ADC_Start */
    hadc1.Init.NbrOfConversion        = 3;           /* rank1=ch16, rank2=ch18, rank3=ch17 */
    hadc1.Init.DiscontinuousConvMode  = DISABLE;
    hadc1.Init.ExternalTrigConv       = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge   = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests  = DISABLE;
    hadc1.Init.Overrun                = ADC_OVR_DATA_PRESERVED;
    hadc1.Init.OversamplingMode       = DISABLE;
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.SamplingTime     = ADC_SAMPLETIME_640CYCLES_5;
    sConfig.SingleDiff       = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber     = ADC_OFFSET_NONE;
    sConfig.Offset           = 0;

    sConfig.Channel = ADC_CHANNEL_TEMPSENSOR_ADC1;   /* ch16 */
    sConfig.Rank    = ADC_REGULAR_RANK_1;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    sConfig.Channel = ADC_CHANNEL_VREFINT;           /* ch18 */
    sConfig.Rank    = ADC_REGULAR_RANK_2;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    sConfig.Channel = ADC_CHANNEL_VBAT;              /* ch17, VBAT/3 */
    sConfig.Rank    = ADC_REGULAR_RANK_3;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    /* Linearity calibration (recommended after every power-up, ~110 us);
     * runs with the ADC disabled - the HAL disables it if needed. */
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ------------------------------------------------------------------------ */
void ADC_Internal_Sample(ADC_InternalResult *res)
{
    HAL_ADC_Start(&hadc1);     /* single scan of 3 conversions */
    HAL_ADC_PollForConversion(&hadc1, 10U);  res->raw_temp    = (uint16_t)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10U);  res->raw_vrefint = (uint16_t)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10U);  res->raw_vbat    = (uint16_t)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
}
