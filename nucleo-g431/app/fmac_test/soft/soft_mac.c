/**
  * @file    soft_mac.c
  * @brief   Software-emulated float FIR (multiply-accumulate) benchmark.
  *
  * Compiled in the fmac_test_soft build (-mfloat-abi=soft): every float
  * add/sub/mul here is a compiler-generated libgcc __aeabi_f* software call.
  */

#include <stdint.h>
#include <string.h>
#include "soft_mac.h"

static volatile uint32_t g_sink;    /* defeats dead-code elimination */

uint32_t Soft_Fir_Bench(const float *x, const float *c, float *out,
                        uint32_t n, uint32_t taps, uint32_t reps)
{
    uint32_t sink = 0U;

    for (uint32_t rep = 0U; rep < reps; rep++)
    {
        for (uint32_t i = 0U; i < n; i++)
        {
            float acc = 0.0f;
            for (uint32_t k = 0U; k < taps; k++)
            {
                float xk = ((int32_t)i >= (int32_t)k) ? x[i - k] : 0.0f;
                acc += c[k] * xk;
            }
            out[i] = acc;

            uint32_t bits;
            memcpy(&bits, &out[i], sizeof(bits));
            sink += bits;
        }
    }
    g_sink = sink;
    return sink;
}
