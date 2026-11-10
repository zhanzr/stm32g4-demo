/*
 * cordic_test - sine benchmark on the Nucleo-G431RB (STM32G431RBT6 @ 170 MHz)
 * (hard-float ABI build; the soft-float variant is the separate
 * cordic_test_soft build in soft/, results merged in the READMEs)
 *
 * Computes sin(x) over a generated list of 32 angles (0..2*pi, step
 * 2*pi/32) with four methods and reports cycles/sin (DWT cycle counter):
 *
 *   1. CORDIC     - the STM32G4 hardware CORDIC IP (q1.31 in/out, HAL polling)
 *   2. CMSIS DSP  - arm_sin_f32() (512-entry table + linear interpolation)
 *   3. FPU float  - newlib sinf(), float ops on the FPU (hard-float ABI)
 *   4. Double     - newlib sin(): the chip has no double-precision FPU, so
 *                   all double math is software (__aeabi_d* routines)
 *
 * All methods are validated against the double-precision sin() and their
 * worst-case absolute error is reported.
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "board.h"
#include "cordic_hw.h"
#include "arm_math.h"

#define PI_D   3.14159265358979323846
#define N_ANG  32U
#define MHZ    170U

/* repetitions per method (sized for ~0.3-1 s per method) */
#define REPS_CORDIC   20000U
#define REPS_CMSIS    20000U
#define REPS_SINF      5000U
#define REPS_DOUBLE    2000U

static float angles_rad[N_ANG];       /* radians, float (methods 2/3/4) */
static int32_t angles_q31[N_ANG];     /* CORDIC q1.31 (2^31 = pi) */

static int32_t res_cordic_q31[N_ANG];
static float res_cmsis[N_ANG];
static float res_sinf[N_ANG];
static double res_sin[N_ANG];         /* reference */

static volatile uint32_t g_sink;      /* global dead-code-elimination guard */

static uint32_t cyc_now(void)
{
    return DWT->CYCCNT;
}

/* Each bench runs `reps` full passes over the input list, fills its result
 * array (final pass), feeds a global sink (defeats dead-code elimination),
 * and returns DWT cycles elapsed. */

static uint32_t bench_cordic(uint32_t reps)
{
    uint32_t s = 0U;
    uint32_t t0 = cyc_now();
    for (uint32_t r = 0; r < reps; r++)
    {
        CORDIC_HW_Sin_List(angles_q31, res_cordic_q31, N_ANG);
    }
    uint32_t t1 = cyc_now();
    for (uint32_t i = 0; i < N_ANG; i++)
    {
        s += (uint32_t)res_cordic_q31[i];
    }
    g_sink += s;
    return t1 - t0;
}

static uint32_t bench_cmsis(uint32_t reps)
{
    uint32_t s = 0U;
    uint32_t t0 = cyc_now();
    for (uint32_t r = 0; r < reps; r++)
    {
        for (uint32_t i = 0; i < N_ANG; i++)
        {
            res_cmsis[i] = arm_sin_f32(angles_rad[i]);
        }
    }
    uint32_t t1 = cyc_now();
    for (uint32_t i = 0; i < N_ANG; i++)
    {
        uint32_t bits;
        memcpy(&bits, &res_cmsis[i], sizeof(bits));
        s += bits;
    }
    g_sink += s;
    return t1 - t0;
}

static uint32_t bench_sinf(uint32_t reps)
{
    uint32_t s = 0U;
    uint32_t t0 = cyc_now();
    for (uint32_t r = 0; r < reps; r++)
    {
        for (uint32_t i = 0; i < N_ANG; i++)
        {
            res_sinf[i] = sinf(angles_rad[i]);
        }
    }
    uint32_t t1 = cyc_now();
    for (uint32_t i = 0; i < N_ANG; i++)
    {
        uint32_t bits;
        memcpy(&bits, &res_sinf[i], sizeof(bits));
        s += bits;
    }
    g_sink += s;
    return t1 - t0;
}

static uint32_t bench_sin_double(uint32_t reps)
{
    uint32_t s = 0U;
    uint32_t t0 = cyc_now();
    for (uint32_t r = 0; r < reps; r++)
    {
        for (uint32_t i = 0; i < N_ANG; i++)
        {
            res_sin[i] = sin((double)angles_rad[i]);
        }
    }
    uint32_t t1 = cyc_now();
    for (uint32_t i = 0; i < N_ANG; i++)
    {
        uint64_t bits = *(uint64_t *)&res_sin[i];
        s += (uint32_t)(bits ^ (bits >> 32));
    }
    g_sink += s;
    return t1 - t0;
}

/* cycles-per-sin for a method: cycles / (reps * N_ANG) */
static void print_timing(uint32_t cycles, uint32_t reps, const char *name)
{
    double per_sin = (double)cycles / ((uint64_t)reps * N_ANG);
    printf("  %-34s : %8.1f cycles/sin (%6.2f us)\r\n",
           name, per_sin, per_sin / MHZ);
}

/* worst-case absolute error of a float result array vs the double reference */
static double max_err_f(const float *res)
{
    double e = 0.0;
    for (uint32_t i = 0; i < N_ANG; i++)
    {
        double d = fabs((double)res[i] - res_sin[i]);
        if (d > e)
        {
            e = d;
        }
    }
    return e;
}

static double max_err_cordic(const int32_t *res_q31)
{
    double e = 0.0;
    for (uint32_t i = 0; i < N_ANG; i++)
    {
        double v = (double)res_q31[i] * (1.0 / 2147483648.0);
        double d = fabs(v - res_sin[i]);
        if (d > e)
        {
            e = d;
        }
    }
    return e;
}

/* q1.31 -> float */
static float q31_to_f(int32_t q)
{
    return (float)q * (1.0f / 2147483648.0f);
}

int main(void)
{
    HAL_Init();
    Board_Init();
    CORDIC_HW_Init();

    /* --- generate the input list: 0..2*pi, step 2*pi/N --------------------- */
    for (uint32_t i = 0; i < N_ANG; i++)
    {
        double deg = (double)i * (360.0 / N_ANG);
        double rad = deg * (PI_D / 180.0);

        angles_rad[i] = (float)rad;

        /* CORDIC q1.31: angle/pi in [0,2) as unsigned 1.31 (wraps at 2*pi) */
        uint32_t u = (uint32_t)((rad / PI_D) * 2147483648.0 + 0.5);
        angles_q31[i] = (int32_t)u;
    }

    printf("\r\n==== cordic_test hard-FPU build (STM32G431RBT6) @ %lu Hz ====\r\n",
           (unsigned long)SystemCoreClock);
    printf("sin(x) over %u angles (0..2pi step 2pi/%u), DWT cycles @ %lu MHz\r\n",
           (unsigned)N_ANG, (unsigned)N_ANG, (unsigned long)MHZ);

    /* --- run all four benchmarks -------------------------------------------- */
    uint32_t cyc_cordic = bench_cordic(REPS_CORDIC);
    uint32_t cyc_cmsis = bench_cmsis(REPS_CMSIS);
    uint32_t cyc_sinf = bench_sinf(REPS_SINF);
    uint32_t cyc_double = bench_sin_double(REPS_DOUBLE);

    /* --- result table -------------------------------------------------------- */
    printf("\r\n  i    deg     CORDIC    CMSIS-DSP   sinf(float)  sin(double)\r\n");
    for (uint32_t i = 0; i < N_ANG; i++)
    {
        double deg = (double)i * (360.0 / N_ANG);
        printf("  %2u  %5.1f   %+2.6f  %+2.6f  %+2.6f   %+2.6f\r\n",
               (unsigned)i, deg,
               (double)q31_to_f(res_cordic_q31[i]),
               (double)res_cmsis[i],
               (double)res_sinf[i],
               res_sin[i]);
    }

    /* --- accuracy (worst-case |method - sin(double)|) ------------------------ */
    printf("\r\nMax |error| vs sin(double):\r\n");
    printf("  CORDIC (q1.31)         : %.3e\r\n", max_err_cordic(res_cordic_q31));
    printf("  CMSIS arm_sin_f32      : %.3e\r\n", max_err_f(res_cmsis));
    printf("  sinf (FPU)             : %.3e\r\n", max_err_f(res_sinf));

    /* --- timing --------------------------------------------------------------- */
    printf("\r\nTiming (DWT cycles, %u angles/pass):\r\n", (unsigned)N_ANG);
    print_timing(cyc_cordic, REPS_CORDIC, "CORDIC HW (HAL polling)");
    print_timing(cyc_cmsis, REPS_CMSIS, "CMSIS arm_sin_f32");
    print_timing(cyc_sinf, REPS_SINF, "sinf (newlib, FPU)");
    print_timing(cyc_double, REPS_DOUBLE, "sin (newlib double, software)");

    printf("\r\ncordic_test done.\r\n");
    while (1)
    {
        LED_TOGGLE();
        HAL_Delay(250);
    }

    return 0;
}
