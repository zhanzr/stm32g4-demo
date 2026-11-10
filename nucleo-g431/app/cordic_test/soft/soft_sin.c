/**
  * @file    soft_sin.c
  * @brief   Software-emulated single-precision sine.
  *
  * Compiled in the cordic_test_soft build (-mfloat-abi=soft): every float
  * add/sub/mul/div is a compiler-generated libgcc __aeabi_f* software call.
  * The sine algorithm itself is the classic FDLIBM float sine: range-reduce
  * the angle to [-pi/4, pi/4] (octant handling) and evaluate a 4-term
  * minimax polynomial (sin or cos kernel).
  */

#include <stdint.h>
#include <string.h>
#include "soft_sin.h"

/* float constants (FDLIBM float k_sinf) */
#define INVPIO2   5.095411522825714e-01f    /* 2/pi */
#define PIO2_1    1.570785522460938e+00f    /* pi/2 first bits (high part) */
#define PIO2_1T   1.080533373462716e-06f    /* pi/2 low tail */

#define S1  -1.6666667163e-01f
#define S2   8.3333337680e-03f
#define S3  -1.9841270114e-04f
#define S4   2.7557314297e-06f

#define C1   4.16666679004e-02f
#define C2  -1.38888892067e-03f
#define C3   2.48015872931e-05f
#define C4  -2.75573141074e-07f

static volatile uint32_t g_sink;    /* defeats dead-code elimination */

static float poly_sin(float x)
{
    float z = x * x;
    float r = z * (S1 + z * (S2 + z * (S3 + z * S4)));
    return x + x * r;
}

static float poly_cos(float x)
{
    float z = x * x;
    return 1.0f - 0.5f * z + z * z * (C1 + z * (C2 + z * (C3 + z * C4)));
}

float Soft_Sin(float x)
{
    /* round(x * 2/pi) -> octant index */
    float kf = x * INVPIO2;
    int32_t n = (int32_t)(kf >= 0.0f ? (kf + 0.5f) : (kf - 0.5f));

    /* two-step argument reduction to [-pi/4, pi/4] */
    x -= (float)n * PIO2_1;
    x -= (float)n * PIO2_1T;

    switch (n & 3)
    {
    case 0:  return poly_sin(x);
    case 1:  return poly_cos(x);
    case 2:  return -poly_sin(x);
    default: return -poly_cos(x);
    }
}

/* ------------------------------------------------------------------------ */
uint32_t Soft_Sin_Bench(const float *angles, float *out,
                        uint32_t count, uint32_t reps)
{
    uint32_t sink = 0U;

    for (uint32_t rep = 0U; rep < reps; rep++)
    {
        for (uint32_t i = 0U; i < count; i++)
        {
            uint32_t bits;
            out[i] = Soft_Sin(angles[i]);
            memcpy(&bits, &out[i], sizeof(bits));
            sink += bits;
        }
    }
    g_sink = sink;
    return sink;
}
