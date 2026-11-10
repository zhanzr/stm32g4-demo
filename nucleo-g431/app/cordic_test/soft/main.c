/*
 * cordic_test_soft - soft-float sine benchmark (STM32G431RBT6 @ 170 MHz)
 *
 * The whole image is compiled with -mfloat-abi=soft (STM32_FLOAT_ABI=soft):
 * every float operation is a libgcc __aeabi_f* software call, simulating a
 * no-FPU device. This is the companion of the hard-float cordic_test build
 * (which cannot mix soft-ABI objects); the READMEs merge the results.
 *
 * Benchmarks over the same 32-angle input list as the hard build:
 *   - sinf, software-emulated (Soft_Sin, FDLIBM-style algorithm)
 *   - sin(), double software math (anchor to align the two builds' results;
 *     the chip has no double FPU, so this is software in both builds)
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "board.h"
#include "soft_sin.h"

#define PI_D   3.14159265358979323846
#define N_ANG  32U
#define MHZ    170U

#define REPS_SOFT     2000U
#define REPS_DOUBLE   2000U

static float angles_rad[N_ANG];
static float res_soft[N_ANG];
static double res_sin[N_ANG];         /* reference */

static volatile uint32_t g_sink;

static uint32_t cyc_now(void)
{
    return DWT->CYCCNT;
}

static uint32_t bench_soft(uint32_t reps)
{
    uint32_t t0 = cyc_now();
    g_sink += Soft_Sin_Bench(angles_rad, res_soft, N_ANG, reps);
    uint32_t t1 = cyc_now();
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

static void print_timing(uint32_t cycles, uint32_t reps, const char *name)
{
    double per_sin = (double)cycles / ((uint64_t)reps * N_ANG);
    printf("  %-34s : %8.1f cycles/sin (%6.2f us)\r\n",
           name, per_sin, per_sin / MHZ);
}

int main(void)
{
    HAL_Init();
    Board_Init();

    /* same input list as the hard build: 0..2*pi, step 2*pi/N */
    for (uint32_t i = 0; i < N_ANG; i++)
    {
        double deg = (double)i * (360.0 / N_ANG);
        angles_rad[i] = (float)(deg * (PI_D / 180.0));
    }

    printf("\r\n==== cordic_test_soft build (STM32G431RBT6) @ %lu Hz, -mfloat-abi=soft ====\r\n",
           (unsigned long)SystemCoreClock);
    printf("sin(x) over %u angles (0..2pi step 2pi/%u), DWT cycles @ %lu MHz\r\n",
           (unsigned)N_ANG, (unsigned)N_ANG, (unsigned long)MHZ);

    uint32_t cyc_soft = bench_soft(REPS_SOFT);
    uint32_t cyc_double = bench_sin_double(REPS_DOUBLE);

    printf("\r\n  i    deg     soft-sinf   sin(double)\r\n");
    for (uint32_t i = 0; i < N_ANG; i++)
    {
        double deg = (double)i * (360.0 / N_ANG);
        printf("  %2u  %5.1f   %+2.6f    %+2.6f\r\n",
               (unsigned)i, deg, (double)res_soft[i], res_sin[i]);
    }

    double err = 0.0;
    for (uint32_t i = 0; i < N_ANG; i++)
    {
        double d = fabs((double)res_soft[i] - res_sin[i]);
        if (d > err)
        {
            err = d;
        }
    }
    printf("\r\nMax |error| soft_sinf vs sin(double): %.3e\r\n", err);

    printf("\r\nTiming (DWT cycles, %u angles/pass):\r\n", (unsigned)N_ANG);
    print_timing(cyc_soft, REPS_SOFT, "sinf (soft-float emulated)");
    print_timing(cyc_double, REPS_DOUBLE, "sin (newlib double, software)");

    printf("\r\ncordic_test_soft done.\r\n");
    while (1)
    {
        LED_TOGGLE();
        HAL_Delay(250);
    }

    return 0;
}
