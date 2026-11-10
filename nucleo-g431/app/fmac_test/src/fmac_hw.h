/**
  * @file    fmac_hw.h
  * @brief   Hardware FMAC (filter math accelerator) FIR wrapper for the
  *          Nucleo-G431RB board.
  *
  * The STM32G4 FMAC computes fixed-point multiply-accumulate streams
  * (FIR/IIR filters) in q1.15 format. This wrapper runs a FIR filter with
  * polled input and output (HAL polling mode) over a continuous stream:
  * preload once, start once, then feed sample chunks and drain results.
  *
  * NOTE: all FMAC buffer base/size register fields are 8 bits (max 255
  * words), so the stream is fed in chunks smaller than that.
  */

#ifndef __FMAC_HW_H__
#define __FMAC_HW_H__

#include <stdint.h>

#define FMAC_TAPS     64U    /* FIR feed-forward taps (P) */
#define FMAC_PRELOAD  (FMAC_TAPS - 1U)    /* history samples to preload */
#define FMAC_STREAM   256U   /* samples per benchmark pass (4 chunks) */
#define FMAC_CHUNK    64U    /* samples per feed/drain chunk */

void FMAC_HW_Init(const int16_t *coeffs_q15, uint32_t r_gain);
void FMAC_HW_Start(void);                        /* zero-history preload + start */
/* Feed n samples (n <= FMAC_STREAM), drain the n results. */
void FMAC_HW_Fir_Block(const int16_t *in_q15, int16_t *out_q15, uint32_t n);
void FMAC_HW_Stop(void);

#endif /* __FMAC_HW_H__ */
