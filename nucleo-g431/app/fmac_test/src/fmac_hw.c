/**
  * @file    fmac_hw.c
  * @brief   Hardware FMAC FIR wrapper (q1.15).
  *
  * HAL is used for configuration, preload, start and stop. The data path is
  * a tight register-level pump (polling FMAC_SR, feeding FMAC->WDATA and
  * draining FMAC->RDATA), which measures the true accelerator throughput -
  * the HAL polling helper drains at most a few words per call and would
  * dominate the measurement.
  *
  * Memory layout of the FMAC RAM (words; all bases/sizes must fit the 8-bit
  * register fields, i.e. be <= 255):
  *   X2 (coefficients) : base 0,  size 64  (P taps)
  *   X1 (input ring)   : base 64, size 127 (63 preload history + 64 room)
  *   Y  (output)       : base 191, size 64
  * Configuration follows ST's FMAC_FIR examples: function = FIR
  * (convolution), R = 1 ("gain = 1", no extra post-shift), clipping enabled.
  */

#include "fmac_hw.h"
#include "board.h"
#include "stm32g4xx_hal.h"

#define X1_BASE     FMAC_TAPS                        /* 64 */
#define X1_SIZE     (FMAC_PRELOAD + FMAC_CHUNK)      /* 127 */
#define Y_BASE      (X1_BASE + X1_SIZE)              /* 191 */
#define Y_SIZE      FMAC_CHUNK                       /* 64 */

static FMAC_HandleTypeDef hfmac;
static volatile uint32_t g_sink_fmac;    /* defeats dead-code elimination */

/* ------------------------------------------------------------------------ */
void FMAC_HW_Init(const int16_t *coeffs_q15, uint32_t r_gain)
{
    FMAC_FilterConfigTypeDef cfg = {0};

    __HAL_RCC_FMAC_CLK_ENABLE();

    hfmac.Instance = FMAC;
    if (HAL_FMAC_Init(&hfmac) != HAL_OK)
    {
        Error_Handler();
    }

    cfg.InputBaseAddress  = X1_BASE;
    cfg.InputBufferSize   = X1_SIZE;
    cfg.InputThreshold    = FMAC_THRESHOLD_1;
    cfg.CoeffBaseAddress  = 0U;
    cfg.CoeffBufferSize   = FMAC_TAPS;
    cfg.OutputBaseAddress = Y_BASE;
    cfg.OutputBufferSize  = Y_SIZE;
    cfg.OutputThreshold   = FMAC_THRESHOLD_1;
    cfg.pCoeffA           = NULL;
    cfg.CoeffASize        = 0U;
    cfg.pCoeffB           = (int16_t *)coeffs_q15;
    cfg.CoeffBSize        = FMAC_TAPS;
    cfg.Filter            = FMAC_FUNC_CONVO_FIR;
    cfg.InputAccess       = FMAC_BUFFER_ACCESS_NONE;
    cfg.OutputAccess      = FMAC_BUFFER_ACCESS_NONE;
    cfg.Clip              = FMAC_CLIP_ENABLED;
    cfg.P                 = FMAC_TAPS;
    cfg.Q                 = 0U;
    cfg.R                 = (uint8_t)r_gain;

    if (HAL_FMAC_FilterConfig(&hfmac, &cfg) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ------------------------------------------------------------------------ */
void FMAC_HW_Start(void)
{
    /* Zero history: warm-up from rest, so FMAC output n matches the plain-C
     * reference FIR (which also assumes zero history) sample for sample. */
    static int16_t zeros[FMAC_PRELOAD] = {0};
    uint16_t n = FMAC_PRELOAD;

    if (HAL_FMAC_FilterPreload(&hfmac, zeros, n, NULL, 0U) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_FMAC_FilterStart(&hfmac, NULL, NULL) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ------------------------------------------------------------------------ */
/* Feed n samples and drain the n results (filter keeps running across
 * calls, so the FIR state chains from one block to the next). */
void FMAC_HW_Fir_Block(const int16_t *in_q15, int16_t *out_q15, uint32_t n)
{
    uint32_t in_pos = 0U;
    uint32_t out_pos = 0U;

    while (out_pos < n)
    {
        uint32_t sr = FMAC->SR;

        if (((sr & FMAC_SR_X1FULL) == 0U) && (in_pos < n))
        {
            FMAC->WDATA = (uint32_t)(uint16_t)in_q15[in_pos];
            in_pos++;
        }
        if (((sr & FMAC_SR_YEMPTY) == 0U) && (out_pos < n))
        {
            out_q15[out_pos] = (int16_t)FMAC->RDATA;
            out_pos++;
        }
    }
    g_sink_fmac += (uint32_t)(int32_t)out_q15[n - 1U];
}

/* ------------------------------------------------------------------------ */
void FMAC_HW_Stop(void)
{
    if (HAL_FMAC_FilterStop(&hfmac) != HAL_OK)
    {
        Error_Handler();
    }
}
