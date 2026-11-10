#include <stdio.h>
#include "board.h"
#include "adc_internal.h"

/* Factory calibration of the internal channels (12-bit codes at
 * VDDA = 3.0 V, stored in system memory by ST):
 *   TS_CAL1     = raw temp-sensor code at 30 C  (0x1FFF75A8)
 *   TS_CAL2     = raw temp-sensor code at 110 C (0x1FFF75CA)
 *   VREFINT_CAL = raw VREFINT code at 3.0 V     (0x1FFF75AA) */
#define TS_CAL1_ADDR  ((uint16_t *)0x1FFF75A8UL)
#define TS_CAL2_ADDR  ((uint16_t *)0x1FFF75CAUL)
#define VREFINT_CAL_VREF_MV  3000U

/* Temperature from the raw temp-sensor code, using the factory calibration.
 * The measured code is first re-scaled to the 3.0 V reference the calibration
 * was done at, then linearly interpolated between the two calibration points:
 *   T = 30 + (adc_scaled - TS_CAL1) * (110 - 30) / (TS_CAL2 - TS_CAL1)  */
static int TempC_FromCode(uint32_t raw_temp, uint32_t vdda_mv)
{
    uint32_t cal1 = *TS_CAL1_ADDR;
    uint32_t cal2 = *TS_CAL2_ADDR;

    if (cal1 == 0U || cal2 <= cal1)
    {
        return 0;   /* no valid calibration data */
    }
    /* Re-scale the measured code to the 3.0 V calibration reference. */
    uint32_t adc_scaled = (raw_temp * vdda_mv) / VREFINT_CAL_VREF_MV;
    if (adc_scaled <= cal1)
    {
        return 30;
    }
    if (adc_scaled >= cal2)
    {
        return 110;
    }
    return 30 + (int)((adc_scaled - cal1) * 80UL / (cal2 - cal1));
}

int main(void)
{
    HAL_Init();
    Board_Init();

    printf("\r\n==== nucleo-g431 (STM32G431RBT6) blink_hello @ 170 MHz ====\r\n");
    printf("SYSCLK = %lu Hz (%lu MHz)\r\n",
           (unsigned long)SystemCoreClock,
           (unsigned long)(SystemCoreClock / 1000000UL));
    printf("LED LD2 = PA5 (high-active), console USART2 PA2/PA3 115200 8-N-1\r\n");
    printf("Hold USER button B1 (PC13) to light LD2, else it blinks\r\n");

    ADC_Internal_Init();

    uint32_t phase = 0;
    uint32_t last_report = 0;

    while (1)
    {
        /* While B1 is held, LD2 follows the button (bring-up check);
         * otherwise it toggles every 250 ms. */
        if (BTN_PRESSED())
        {
            LED_ON();
        }
        else
        {
            LED_TOGGLE();
        }
        phase++;
        HAL_Delay(250);

        /* Every 4 phases (~1 s): sample the ADC1 internal channels. */
        if (phase % 4 == 0)
        {
            uint32_t now = HAL_GetTick();
            if (now - last_report >= 1000)
            {
                last_report = now;

                ADC_InternalResult adc;
                ADC_Internal_Sample(&adc);

                /* Actual supply voltage from VREFINT: Vdda = VREFINT_CAL_VREF
                 * * VREFINT_CAL / code (VREFINT_CAL_ADDR = 0x1FFF75AA). */
                uint32_t vdda_mv = 0;
                if (adc.raw_vrefint != 0U)
                {
                    vdda_mv = (VREFINT_CAL_VREF_MV * (*VREFINT_CAL_ADDR)) / adc.raw_vrefint;
                }

                /* Junction temperature from the factory-calibrated temp sensor. */
                int temp_c = TempC_FromCode(adc.raw_temp, vdda_mv);

                /* VBAT voltage: the G4 VBAT channel measures VBAT/3
                 * (internal 1/3 divider before IN17), so scale by 3. */
                uint32_t vbat_mv = (vdda_mv * adc.raw_vbat) / 4095UL * 3UL;

                printf("ADC1: temp=%hu code, VREFINT=%hu code, VBAT=%hu code\r\n",
                       adc.raw_temp, adc.raw_vrefint, adc.raw_vbat);
                printf("     Vdda ~= %lu mV, chip temp ~= %d C, VBAT ~= %lu mV\r\n",
                       (unsigned long)vdda_mv, temp_c, (unsigned long)vbat_mv);
            }
        }
    }

    return 0;
}
