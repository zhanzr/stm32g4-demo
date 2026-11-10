/**
  * @file    soft_mac.h
  * @brief   Software-emulated float FIR benchmark (soft-float ABI build).
  *
  * Lives in the fmac_test_soft build (whole image compiled with
  * -mfloat-abi=soft): every float operation is a libgcc __aeabi_f* software
  * call, simulating the no-FPU case.
  */

#ifndef __SOFT_MAC_H__
#define __SOFT_MAC_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Direct-form float FIR over n samples (taps coefficients, zero history).
 * out[] holds the results from the final pass. reps = passes over the same
 * stream. Returns an internal sink (defeats dead-code elimination). */
uint32_t Soft_Fir_Bench(const float *x, const float *c, float *out,
                        uint32_t n, uint32_t taps, uint32_t reps);

#ifdef __cplusplus
}
#endif

#endif /* __SOFT_MAC_H__ */
