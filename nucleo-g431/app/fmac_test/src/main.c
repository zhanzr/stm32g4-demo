/*
 * fmac_test - FMAC (filter math accelerator) benchmark on the
 * Nucleo-G431RB (STM32G431RBT6 @ 170 MHz). Hard-float ABI build; the
 * soft-float variant is the separate fmac_test_soft build in soft/
 * (results merged in the README).
 *
 * Workload: a generated 64-tap FIR filter over a generated 256-sample
 * q1.15 two-tone stream, computed with four methods:
 *
 *   1. FMAC HW    - the STM32G4 FMAC accelerator (q1.15 MACs, register-
 *                   level polling pump)
 *   2. CPU q1.15  - plain-C integer FIR (int16*x int16 -> int32 MAC, >>15)
 *   3. FPU float  - plain-C float FIR (hard-float ABI, FPU arithmetic)
 *   4. Double     - plain-C double FIR (software double math on this chip)
 *
 * Inputs, coefficients and per-sample results are printed over the console;
 * timing (DWT cycle counter) is reported as cycles per output sample (one
 * output sample = P=64 multiply-accumulates).
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "board.h"
#include "fmac_hw.h"

#define PI_D     3.14159265358979323846
#define N_SAMP   256U     /* stream length per pass */
#define N_TAPS   64U      /* FIR taps */
#define HIST     (N_TAPS - 1U)
#define MHZ      170U

/* repetitions per method (sized for ~50-150 ms per method) */
#define REPS_FMAC   500U
#define REPS_Q15    100U
#define REPS_FLT    100U
#define REPS_DBL     20U

static int16_t x15[N_SAMP];        /* input stream, q1.15 */
static int16_t c15[N_TAPS];        /* coefficients, q1.15 */
static float   xf[N_SAMP];         /* input as float (= x15/32768) */
static float   cf[N_TAPS];         /* coefficients as float */
static double  xd[N_SAMP];         /* input as double (exact quantized) */
static double  cd[N_TAPS];         /* coefficients as double (exact quantized) */

static int16_t res_fmac[N_SAMP];
static int16_t res_q15[N_SAMP];
static float   res_flt[N_SAMP];
static double  res_dbl[N_SAMP];    /* reference */
static int16_t scratch_fmac[N_SAMP];  /* timed FMAC passes (stream continues) */

static volatile uint32_t g_sink;

static uint32_t cyc_now(void)
{
    return DWT->CYCCNT;
}

static void sat16_store(int16_t *dst, int32_t v)
{
    if (v > 32767)
    {
        v = 32767;
    }
    if (v < -32768)
    {
        v = -32768;
    }
    *dst = (int16_t)v;
}

/* plain-C direct-form FIR helper: y[n] = sum(c[k]*x[n-k]), zero history */
static void fir_q15_cpu(const int16_t *in, int16_t *out, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
    {
        int32_t acc = 0;
        for (uint32_t k = 0; k < N_TAPS; k++)
        {
            int32_t xk = ((int32_t)i >= (int32_t)k) ? in[i - k] : 0;
            acc += (int32_t)c15[k] * xk;
        }
        sat16_store(&out[i], acc >> 15);
    }
}

static void fir_flt_cpu(const float *in, float *out, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
    {
        float acc = 0.0f;
        for (uint32_t k = 0; k < N_TAPS; k++)
        {
            float xk = ((int32_t)i >= (int32_t)k) ? in[i - k] : 0.0f;
            acc += cf[k] * xk;
        }
        out[i] = acc;
    }
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

static uint32_t bench_fmac(uint32_t reps, int16_t *out)
{
    uint32_t s = 0;
    uint32_t t0 = cyc_now();
    for (uint32_t r = 0; r < reps; r++)
    {
        FMAC_HW_Fir_Block(x15, out, N_SAMP);
    }
    uint32_t t1 = cyc_now();
    for (uint32_t i = 0; i < N_SAMP; i++)
    {
        s += (uint32_t)(int32_t)out[i];
    }
    g_sink += s;
    return t1 - t0;
}

static uint32_t bench_q15(uint32_t reps)
{
    uint32_t t0 = cyc_now();
    for (uint32_t r = 0; r < reps; r++)
    {
        fir_q15_cpu(x15, res_q15, N_SAMP);
    }
    uint32_t t1 = cyc_now();
    uint32_t s = 0;
    for (uint32_t i = 0; i < N_SAMP; i++)
    {
        s += (uint32_t)(int32_t)res_q15[i];
    }
    g_sink += s;
    return t1 - t0;
}

static uint32_t bench_flt(uint32_t reps)
{
    uint32_t s = 0;
    uint32_t t0 = cyc_now();
    for (uint32_t r = 0; r < reps; r++)
    {
        fir_flt_cpu(xf, res_flt, N_SAMP);
    }
    uint32_t t1 = cyc_now();
    for (uint32_t i = 0; i < N_SAMP; i++)
    {
        uint32_t bits;
        memcpy(&bits, &res_flt[i], sizeof(bits));
        s += bits;
    }
    g_sink += s;
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

    /* --- generate coefficients: hann-windowed sinc LPF, fc = 0.125 -------- */
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
        double v = ctmp[k] / csum;            /* unity DC gain */
        int32_t q = (int32_t)(v * 32768.0 + (v >= 0.0 ? 0.5 : -0.5));
        c15[k] = (int16_t)q;
        cf[k] = (float)((double)c15[k] / 32768.0);
        cd[k] = (double)c15[k] / 32768.0;
    }

    /* --- generate the input stream: two-tone signal, q1.15 ----------------- */
    for (uint32_t n = 0; n < N_SAMP; n++)
    {
        double t = (double)n / 64.0;   /* sample index scaled for tones */
        double v = 0.5 * sin(2.0 * PI_D * 2.0 * t)      /* 2 cycles / 64: in-band */
                 + 0.25 * sin(2.0 * PI_D * 15.0 * t);   /* 15 cycles / 64: out-of-band */
        int32_t q = (int32_t)(v * 32768.0);
        if (q > 32767)  { q = 32767;  }
        if (q < -32768) { q = -32768; }
        x15[n] = (int16_t)q;
        xf[n] = (float)((double)x15[n] / 32768.0);
        xd[n] = (double)x15[n] / 32768.0;
    }

    printf("\r\n==== fmac_test hard-FPU build (STM32G431RBT6) @ %lu Hz ====\r\n",
           (unsigned long)SystemCoreClock);
    printf("64-tap FIR over %u generated q1.15 samples, DWT cycles @ %lu MHz\r\n",
           (unsigned)N_SAMP, (unsigned long)MHZ);

    /* --- FMAC hardware: validation pass first (zero-history preload, so the
     * FMAC outputs match the plain-C reference sample for sample), then the
     * timed passes continue the same stream into a scratch buffer ---------- */
    FMAC_HW_Init(c15, 0U);       /* R = 0: no extra gain (q1.15 in -> q1.15 out) */
    FMAC_HW_Start();
    FMAC_HW_Fir_Block(x15, res_fmac, N_SAMP);
    uint32_t cyc_fmac = bench_fmac(REPS_FMAC, scratch_fmac);
    FMAC_HW_Stop();

    /* --- CPU reference methods ---------------------------------------------- */
    uint32_t cyc_q15 = bench_q15(REPS_Q15);
    uint32_t cyc_flt = bench_flt(REPS_FLT);
    uint32_t cyc_dbl = bench_dbl(REPS_DBL);

    /* --- result table -------------------------------------------------------- */
    printf("\r\n  n    x15     FMAC     q15-CPU    float     double\r\n");
    for (uint32_t i = 0; i < 16U; i++)
    {
        printf("  %2u  %6d  %6d  %6d  %+2.6f  %+2.6f\r\n",
               (unsigned)i, (int)x15[i], (int)res_fmac[i], (int)res_q15[i],
               (double)res_flt[i], res_dbl[i]);
    }

    /* --- validation ----------------------------------------------------------- */
    int32_t max_lsb = 0;
    for (uint32_t i = 0; i < N_SAMP; i++)
    {
        int32_t d = (int32_t)res_fmac[i] - (int32_t)res_q15[i];
        if (d < 0) { d = -d; }
        if (d > max_lsb) { max_lsb = d; }
    }
    double err = 0.0;
    for (uint32_t i = 0; i < N_SAMP; i++)
    {
        double d = fabs(res_flt[i] - res_dbl[i]);
        if (d > err) { err = d; }
    }
    printf("\r\nMax |FMAC - q15-CPU|: %ld LSB   (fixed-point agreement)\r\n", (long)max_lsb);
    printf("Max |float FIR - double FIR|: %.3e\r\n", err);

    /* --- timing ----------------------------------------------------------------- */
    printf("\r\nTiming (DWT cycles, %u samples/pass, 1 sample = %u MACs):\r\n",
           (unsigned)N_SAMP, (unsigned)N_TAPS);
    print_timing(cyc_fmac, REPS_FMAC, "FMAC HW (register pump)");
    print_timing(cyc_q15, REPS_Q15, "CPU q1.15 FIR (plain C)");
    print_timing(cyc_flt, REPS_FLT, "CPU float FIR (FPU)");
    print_timing(cyc_dbl, REPS_DBL, "CPU double FIR (software)");

    printf("\r\nfmac_test done.\r\n");
    while (1)
    {
        LED_TOGGLE();
        HAL_Delay(250);
    }

    return 0;
}
