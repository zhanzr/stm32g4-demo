/*
 * fmac_test_soft - soft-float FIR benchmark (STM32G431RBT6 @ 170 MHz)
 *
 * The whole image is compiled with -mfloat-abi=soft (STM32_FLOAT_ABI=soft):
 * every float operation is a libgcc __aeabi_f* software call, simulating a
 * no-FPU device. Companion of the hard-float fmac_test build (soft and hard
 * ABI objects cannot be mixed in one binary); the READMEs merge results.
 *
 * Benchmarks the same 64-tap FIR over the same generated 256-sample
 * q1.15-derived two-tone stream as the hard build:
 *   - float FIR, software-emulated arithmetic (Soft_Fir_Bench)
 *   - double FIR, software math (anchor to align the two builds)
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "board.h"
#include "soft_mac.h"

#define PI_D     3.14159265358979323846
#define N_SAMP   256U
#define N_TAPS   64U
#define MHZ      170U

#define REPS_SOFT   20U
#define REPS_DBL    20U

static float   xf[N_SAMP];
static float   cf[N_TAPS];
static double  xd[N_SAMP];
static double  cd[N_TAPS];
static float   res_soft[N_SAMP];
static double  res_dbl[N_SAMP];

static volatile uint32_t g_sink;

static uint32_t cyc_now(void)
{
    return DWT->CYCCNT;
}

static void fir_dbl_cpu(const double *in, double *out, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
    {
        double acc = 0.0;
        for (uint32_t k = 0; k < N_TAPS; k++)
        {
            double xk = ((int32_t)i >= (int32_t)k) ? in[i - k] : 0.0;
            acc += cd[k] * xk;
        }
        out[i] = acc;
    }
}

static uint32_t bench_soft(uint32_t reps)
{
    uint32_t t0 = cyc_now();
    g_sink += Soft_Fir_Bench(xf, cf, res_soft, N_SAMP, N_TAPS, reps);
    uint32_t t1 = cyc_now();
    return t1 - t0;
}

static uint32_t bench_dbl(uint32_t reps)
{
    uint32_t s = 0;
    uint32_t t0 = cyc_now();
    for (uint32_t r = 0; r < reps; r++)
    {
        fir_dbl_cpu(xd, res_dbl, N_SAMP);
    }
    uint32_t t1 = cyc_now();
    for (uint32_t i = 0; i < N_SAMP; i++)
    {
        uint64_t bits = *(uint64_t *)&res_dbl[i];
        s += (uint32_t)(bits ^ (bits >> 32));
    }
    g_sink += s;
    return t1 - t0;
}

static void print_timing(uint32_t cycles, uint32_t reps, const char *name)
{
    double per_samp = (double)cycles / ((uint64_t)reps * N_SAMP);
    printf("  %-30s : %8.1f cycles/sample (%6.2f us, %5.2f cycles/MAC)\r\n",
           name, per_samp, per_samp / MHZ, per_samp / N_TAPS);
}

int main(void)
{
    HAL_Init();
    Board_Init();

    /* same generated data as the hard build */
    double csum = 0.0;
    double ctmp[N_TAPS];
    for (uint32_t k = 0; k < N_TAPS; k++)
    {
        double m = (double)k - (double)(N_TAPS - 1) / 2.0;
        double sinc = (m == 0.0) ? (2.0 * 0.125)
                                 : sin(2.0 * PI_D * 0.125 * m) / (PI_D * m);
        double win = 0.5 - 0.5 * cos(2.0 * PI_D * (double)k / (double)(N_TAPS - 1));
        ctmp[k] = sinc * win;
        csum += ctmp[k];
    }
    for (uint32_t k = 0; k < N_TAPS; k++)
    {
        double v = ctmp[k] / csum;
        int32_t q = (int32_t)(v * 32768.0 + (v >= 0.0 ? 0.5 : -0.5));
        cf[k] = (float)((double)q / 32768.0);
        cd[k] = (double)q / 32768.0;
    }
    for (uint32_t n = 0; n < N_SAMP; n++)
    {
        double t = (double)n / 64.0;
        double v = 0.5 * sin(2.0 * PI_D * 2.0 * t)
                 + 0.25 * sin(2.0 * PI_D * 15.0 * t);
        int32_t q = (int32_t)(v * 32768.0);
        if (q > 32767)  { q = 32767;  }
        if (q < -32768) { q = -32768; }
        xf[n] = (float)((double)q / 32768.0);
        xd[n] = (double)q / 32768.0;
    }

    printf("\r\n==== fmac_test_soft build (STM32G431RBT6) @ %lu Hz, -mfloat-abi=soft ====\r\n",
           (unsigned long)SystemCoreClock);
    printf("64-tap FIR over %u generated samples, DWT cycles @ %lu MHz\r\n",
           (unsigned)N_SAMP, (unsigned long)MHZ);

    uint32_t cyc_soft = bench_soft(REPS_SOFT);
    uint32_t cyc_dbl = bench_dbl(REPS_DBL);

    printf("\r\n  n    soft-FIR     double-FIR\r\n");
    for (uint32_t i = 0; i < 16U; i++)
    {
        printf("  %2u  %+2.6f  %+2.6f\r\n",
               (unsigned)i, (double)res_soft[i], res_dbl[i]);
    }

    double err = 0.0;
    for (uint32_t i = 0; i < N_SAMP; i++)
    {
        double d = fabs((double)res_soft[i] - res_dbl[i]);
        if (d > err) { err = d; }
    }
    printf("\r\nMax |float FIR - double FIR|: %.3e\r\n", err);

    printf("\r\nTiming (DWT cycles, %u samples/pass, 1 sample = %u MACs):\r\n",
           (unsigned)N_SAMP, (unsigned)N_TAPS);
    print_timing(cyc_soft, REPS_SOFT, "float FIR (soft-float emulated)");
    print_timing(cyc_dbl, REPS_DBL, "double FIR (software)");

    printf("\r\nfmac_test_soft done.\r\n");
    while (1)
    {
        LED_TOGGLE();
        HAL_Delay(250);
    }

    return 0;
}
