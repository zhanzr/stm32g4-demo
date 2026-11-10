/**
  * @file    cordic_hw.h
  * @brief   Hardware CORDIC IP (sine) wrapper for the Nucleo-G431RB board.
  *
  * The STM32G4 CORDIC accelerator computes sin(angle) in q1.31 fixed point
  * (input: angle in q1.31 with 2^31 = pi; output: sin in q1.31).
  * This wrapper uses the HAL in polling (blocking) mode.
  */

#ifndef __CORDIC_HW_H__
#define __CORDIC_HW_H__

#include <stdint.h>

void CORDIC_HW_Init(void);
/* Compute sine for a list of q1.31 angles (blocking, HAL polling mode). */
void CORDIC_HW_Sin_List(const int32_t *in_q31, int32_t *out_q31, uint32_t len);

#endif /* __CORDIC_HW_H__ */
