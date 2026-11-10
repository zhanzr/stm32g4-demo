/**
  * @file    adc_internal.h
  * @brief   ADC1 internal-channel sampling for the Nucleo-G431RB board.
  *
  * Internal channels of ADC1 on STM32G431:
  *   - CH16  temperature sensor
  *   - CH17  VBAT (internal 1/3 divider -> VBAT = code * Vdda / 4095 * 3)
  *   - CH18  VREFINT (~1.21 V; factory-calibrated at VDDA = 3.0 V)
  *
  * Unlike F4 (shared IN18), on G4 all three internal paths are independent
  * and can be converted in a single scan.
  */

#ifndef __ADC_INTERNAL_H__
#define __ADC_INTERNAL_H__

#include <stdint.h>

typedef struct {
    uint16_t raw_temp;    /* ADC1 ch16 code (temperature sensor) */
    uint16_t raw_vrefint; /* ADC1 ch18 code (internal reference) */
    uint16_t raw_vbat;    /* ADC1 ch17 code (VBAT/3) */
} ADC_InternalResult;

void ADC_Internal_Init(void);
void ADC_Internal_Sample(ADC_InternalResult *res);

#endif /* __ADC_INTERNAL_H__ */
