#include <stdio.h>
#include "board.h"

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

        /* Heartbeat message every 4 phases (~1 s). */
        if (phase % 4 == 0)
        {
            uint32_t now = HAL_GetTick();
            if (now - last_report >= 1000)
            {
                last_report = now;
                printf("blink_hello alive, tick = %lu ms\r\n", (unsigned long)now);
            }
        }
    }

    return 0;
}
