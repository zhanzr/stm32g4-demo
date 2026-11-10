/**
  * @file    board.c
  * @brief   Board init for the Nucleo-G431RB board (STM32G431RBT6).
  *
  * Clock tree (HSI = 16 MHz - the Nucleo-64 has no HSE crystal fitted; the
  * ST-LINK MCO 8 MHz signal only reaches PF0/PH0 through solder bridges,
  * so the internal oscillator is the reliable source):
  *   VOS Range 1 BOOST (required above 150 MHz on STM32G4)
  *   PLLM=4   -> PLL input  4 MHz  (2.66..8 MHz window)
  *   PLLN=85  -> VCO      340 MHz  (96..344 MHz window)
  *   PLLR=2   -> SYSCLK   170 MHz  (G431 family maximum)
  *   AHB=170 MHz, APB1=170 MHz (/1, max), APB2=170 MHz (/1, max)
  *   Flash latency 4 wait states (boost range @ 170 MHz).
  *
  * LED (Nucleo-64): LD2 = PA5 (green), HIGH-active (pin SET = LED on).
  *
  * Console: USART2 PA2 (TX) / PA3 (RX) - the ST-LINK VCP wiring on the
  * Nucleo-64 (115200 8-N-1).
  *
  * SystemClock_Config() is declared weak so an individual project can bring
  * in its own clock setup without affecting the default 170 MHz used by
  * every other project.
  */

#include "board.h"
#include "uart_printf.h"
#include "swv_printf.h"

/* ------------------------------------------------------------------------ */
__attribute__((weak)) void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();

    /* Range 1 BOOST regulator is mandatory above 150 MHz on STM32G4. */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST) != HAL_OK)
    {
        Error_Handler();
    }

    /* HSI-driven PLL (the Nucleo-64 has no HSE crystal fitted; whether the
     * ST-LINK MCO actually reaches PF0 depends on solder bridges). */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM            = RCC_PLLM_DIV4;   /*  4 MHz input  */
    RCC_OscInitStruct.PLL.PLLN            = 85U;             /* 340 MHz VCO   */
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;   /* 170 MHz SYSCLK */
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* 170 MHz (max) */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;     /* 170 MHz (max) */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;     /* 170 MHz (max) */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ------------------------------------------------------------------------ */
static void GPIO_LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* LD2 = PA5: push-pull output, HIGH = ON. Start OFF (low). */
    GPIO_InitStruct.Pin   = LED_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

    LED_OFF();
}

/* ------------------------------------------------------------------------ */
static void GPIO_Button_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* USER button B1 = PC13. On the Nucleo-64 pressing B1 pulls the pin
     * HIGH (to 3V3); input with pull-down so the idle level reads LOW. */
    GPIO_InitStruct.Pin   = BTN_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BTN_GPIO_Port, &GPIO_InitStruct);
}

/* ------------------------------------------------------------------------ */
void Board_Init(void)
{
    SystemClock_Config();
    GPIO_LED_Init();
    GPIO_Button_Init();
    UART_Init();
    SWV_Init();
}

/* ------------------------------------------------------------------------ */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
        /* Blink the LED as a fatal-error indicator. */
        LED_TOGGLE();
        for (volatile uint32_t i = 0; i < 1000000UL; i++) { }
    }
}
