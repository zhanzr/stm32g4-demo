/**
  * @file    soft_sin.h
  * @brief   Software-emulated single-precision sine (soft-float ABI build).
  *
  * This file lives in the cordic_test_soft build (whole image compiled with
  * -mfloat-abi=soft): every float operation is a libgcc __aeabi_f* software
  * call, simulating the no-FPU case.
  */

#ifndef __SOFT_SIN_H__
#define __SOFT_SIN_H__

#ifdef __cplusplus
extern "C" {
#endif

/* FDLIBM-style float sine (range reduce to [-pi/4, pi/4], minimax poly). */
float Soft_Sin(float x);

/* Run Soft_Sin() over `count` angles, `reps` times; out[] holds the results
 * from the final pass. Returns an internal sink (defeats dead-code
 * elimination). */
uint32_t Soft_Sin_Bench(const float *angles, float *out,
                        uint32_t count, uint32_t reps);

#ifdef __cplusplus
}
#endif

#endif /* __SOFT_SIN_H__ */
