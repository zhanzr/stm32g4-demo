/*
  backlight.h - LCD backlight PWM (BL = PB11, TIM2_CH4), 0..100% duty.
*/

#ifndef __BACKLIGHT_H
#define __BACKLIGHT_H

#include <stdint.h>

void Backlight_Init(void);
void Backlight_SetDuty(uint16_t percent);
uint16_t Backlight_GetDuty(void);

#endif /* __BACKLIGHT_H */
