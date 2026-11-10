/**
  * @file    board.h
  * @brief   Board support for the Nucleo-G431RB board (STM32G431RBT6).
  *
  * LED:   LD2-PA5 (green) - high-active (pin SET = LED on).
  * Button: USER B1-PC13, active-high (pressed = 3V3, read low otherwise with
  *         the pull-down the board layer installs).
  * Console: USART2 PA2 (TX) / PA3 (RX), 115200 8-N-1 - the ST-LINK VCP.
  */

#ifndef __BOARD_H__
#define __BOARD_H__

#include "stm32g4xx_hal.h"

/* --- LED (high-active: pin SET = ON) ---------------------------------------- */
#define LED_Pin         GPIO_PIN_5      /* LD2 green (also the Arduino D13 pin) */
#define LED_GPIO_Port   GPIOA

#define LED_ON()        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET)
#define LED_OFF()       HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET)
#define LED_TOGGLE()    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin)

/* --- User button (B1 = PC13, pressed -> 3V3 = high) ------------------------ */
#define BTN_Pin         GPIO_PIN_13
#define BTN_GPIO_Port   GPIOC

#define BTN_PRESSED()   (HAL_GPIO_ReadPin(BTN_GPIO_Port, BTN_Pin) == GPIO_PIN_SET)

/* --- Init ------------------------------------------------------------------ */
void Board_Init(void);          /* clocks (170 MHz HSI PLL, G431 max), LED, button, UART (USART2 PA2/PA3) */
void SystemClock_Config(void);  /* HSI 16 MHz -> PLL -> 170 MHz */
void Error_Handler(void);

#endif /* __BOARD_H__ */
