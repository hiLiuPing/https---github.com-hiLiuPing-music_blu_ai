#define MUSIC_FFT_DEBUG    1U

#include "music_fft.h"

#include "music_app.h"
#include "log.h"
#include "sai.h"

#include <math.h>
#include <string.h>

#if !defined(ARM_MATH_CM4)
#define ARM_MATH_CM4
#endif
#include "arm_math.h"

#define MUSIC_FFT_CHANNELS          2U
#define MUSIC_FFT_PI                3.14159265358979323846f
#define MUSIC_FFT_NO_SIGNAL_FRAMES  18U

/*
 * Two independent ping-pong DMA buffers.
 * Each holds MUSIC_FFT_SIZE stereo frames (one FFT window).
 * DMA (NORMAL mode) fills one buffer while the FFT reads the other.
 * No circular wrap, no data race.
 */
static uint32_t s_dma_buf[2][MUSIC_FFT_SIZE];
static float s_fft_in[MUSIC_FFT_SIZE];
static float s_window[MUSIC_FFT_SIZE];
static float s_mag[MUSIC_FFT_SIZE / 2U];
static uint8_t s_bars[MUSIC_FFT_BAR_COUNT];
static uint8_t s_initialized;

/* Double-buffer sync: ISR sets buf_ready_idx, FFT loop consumes it. */

static uint8_t s_silent_frames;
static arm_rfft_fast_instance_f32 s_rfft;
static float s_fft_out[MUSIC_FFT_SIZE];
static const uint16_t s_bar_edges[MUSIC_FFT_BAR_COUNT + 1U] = {
    2U, 4U, 7U, 11U, 17U, 26U, 39U, 59U, (MUSIC_FFT_SIZE / 2U)
};

/* 0xFF = no buffer ready; 0/1 = buffer ready for FFT processing. */
#define FFT_BUF_IDLE  0xFFU
static volatile uint8_t s_buf_ready_idx;

static void MusicFFT_ResetBars(void)
{
    memset(s_bars, 0, sizeof(s_bars));
    s_silent_frames = MUSIC_FFT_NO_SIGNAL_FRAMES;
}

static uint16_t MusicFFT_BinToBar(uint16_t bin)
{
    uint8_t bar;

    for (bar = 0U; bar < MUSIC_FFT_BAR_COUNT; bar++)
    {
        if (bin < s_bar_edges[bar + 1U])
        {
            return bar;
        }
    }

    return MUSIC_FFT_BAR_COUNT - 1U;
}

static uint8_t MusicFFT_LevelToBar(float level)
{
    float scaled;

    if (level <= 0.0f)
    {
        return 0U;
    }

    scaled = log10f(1.0f + level) * 11.0f;
    if (scaled > (float)MUSIC_FFT_BAR_MAX)
    {
        scaled = (float)MUSIC_FFT_BAR_MAX;
    }

    return (uint8_t)(scaled + 0.5f);
}

static void MusicFFT_DecayBars(void)
{
    uint8_t i;

    for (i = 0U; i < MUSIC_FFT_BAR_COUNT; i++)
    {
        if (s_bars[i] > 1U)
        {
            s_bars[i] = (uint8_t)(s_bars[i] - 2U);
        }
        else
        {
            s_bars[i] = 0U;
        }
    }
}

static void MusicFFT_BuildInput(const uint32_t *src)
{
    uint16_t i;
    int64_t dc = 0;

    for (i = 0U; i < MUSIC_FFT_SIZE; i++)
    {
        uint32_t frame = src[i];
        int16_t left = (int16_t)(frame & 0xFFFFU);
        int16_t right = (int16_t)((frame >> 16U) & 0xFFFFU);
        int32_t mixed = ((int32_t)left + (int32_t)right) / 2;

        s_fft_in[i] = (float)mixed;
        dc += mixed;
    }

    dc /= (int64_t)MUSIC_FFT_SIZE;

    for (i = 0U; i < MUSIC_FFT_SIZE; i++)
    {
        s_fft_in[i] = (s_fft_in[i] - (float)dc) * s_window[i];
    }
}

static void MusicFFT_RunFft(void)
{
    arm_rfft_fast_f32(&s_rfft, s_fft_in, s_fft_out, 0);
    s_mag[0] = fabsf(s_fft_out[0]);

    arm_cmplx_mag_f32(&s_fft_out[2], &s_mag[1], (MUSIC_FFT_SIZE / 2U) - 1U);
}

static void MusicFFT_UpdateBars(void)
{
    float sums[MUSIC_FFT_BAR_COUNT] = {0.0f};
    uint8_t counts[MUSIC_FFT_BAR_COUNT] = {0U};
    uint8_t next[MUSIC_FFT_BAR_COUNT] = {0U};
    uint16_t bin;
    uint8_t i;
    uint8_t active = 0U;

    for (bin = 2U; bin < (MUSIC_FFT_SIZE / 2U); bin++)
    {
        uint16_t bar = MusicFFT_BinToBar(bin);
        sums[bar] += s_mag[bin];
        counts[bar]++;
    }

    for (i = 0U; i < MUSIC_FFT_BAR_COUNT; i++)
    {
        float avg = (counts[i] == 0U) ? 0.0f : (sums[i] / (float)counts[i]);
        uint8_t level = MusicFFT_LevelToBar(avg / 12000.0f);

        next[i] = level;
        if (level > 1U)
        {
            active = 1U;
        }
    }

    if (active)
    {
        s_silent_frames = 0U;
    }
    else if (s_silent_frames < MUSIC_FFT_NO_SIGNAL_FRAMES)
    {
        s_silent_frames++;
    }

    for (i = 0U; i < MUSIC_FFT_BAR_COUNT; i++)
    {
        if (next[i] >= s_bars[i])
        {
            s_bars[i] = next[i];
        }
        else
        {
            s_bars[i] = (uint8_t)(((uint16_t)s_bars[i] * 3U + (uint16_t)next[i]) / 4U);
        }
    }
}

HAL_StatusTypeDef MusicFFT_Init(void)
{
    uint16_t i;
    HAL_StatusTypeDef status;

    if (s_initialized)
    {
        return HAL_OK;
    }

    memset(s_dma_buf, 0, sizeof(s_dma_buf));
    memset(s_fft_in, 0, sizeof(s_fft_in));
    memset(s_mag, 0, sizeof(s_mag));
    MusicFFT_ResetBars();
    s_buf_ready_idx = FFT_BUF_IDLE;

    for (i = 0U; i < MUSIC_FFT_SIZE; i++)
    {
        s_window[i] = 0.5f - (0.5f * cosf((2.0f * MUSIC_FFT_PI * (float)i) / (float)(MUSIC_FFT_SIZE - 1U)));
    }

    if (arm_rfft_fast_init_f32(&s_rfft, MUSIC_FFT_SIZE) != ARM_MATH_SUCCESS)
    {
        return HAL_ERROR;
    }

    /* Start first DMA transfer into buf[0] (NORMAL mode, one-shot). */
    status = HAL_SAI_Receive_DMA(&hsai_BlockA1, (uint8_t *)s_dma_buf[0], MUSIC_FFT_SIZE);
    if (status == HAL_OK)
    {
        s_initialized = 1U;
    }

    return status;
}

void MusicFFT_Start(void)
{
    /* DMA always runs; just reset the silence counter. */
    s_silent_frames = 0U;
}

void MusicFFT_Stop(void)
{
    MusicFFT_ResetBars();
}

void MusicFFT_Process(void)
{
    uint8_t ready;

    if (!s_initialized)
    {
        return;
    }

    if (!g_music_ble_state.music_played)
    {
        MusicFFT_DecayBars();
        if (s_silent_frames < MUSIC_FFT_NO_SIGNAL_FRAMES)
        {
            s_silent_frames++;
        }
        return;
    }

    ready = s_buf_ready_idx;
    if (ready != FFT_BUF_IDLE)
    {
        s_buf_ready_idx = FFT_BUF_IDLE;
        MusicFFT_BuildInput(s_dma_buf[ready]);
        MusicFFT_RunFft();
        MusicFFT_UpdateBars();

#if MUSIC_FFT_DEBUG
        {
            static uint16_t s_dbg_cnt;
            uint32_t frame;
            int16_t l0, r0, l1, r1;
            uint8_t bi;

            s_dbg_cnt++;
            /* Print every 5 frames */
            if ((s_dbg_cnt % 5U) == 1U)
            {
                frame = s_dma_buf[ready][0];
                l0 = (int16_t)(frame & 0xFFFFU);
                r0 = (int16_t)((frame >> 16U) & 0xFFFFU);
                frame = s_dma_buf[ready][1];
                l1 = (int16_t)(frame & 0xFFFFU);
                r1 = (int16_t)((frame >> 16U) & 0xFFFFU);

                log_printf("[FFT#%u] L=%d R=%d L=%d R=%d DC=%ld ready=%u\r\n",
                    s_dbg_cnt,
                    (int)l0, (int)r0, (int)l1, (int)r1,
                    (long)((int64_t)s_fft_in[0] - (int64_t)(int32_t)(s_fft_in[0] * s_window[0])),
                    ready);

                log_printf("[FFT] mag=");
                for (bi = 0U; bi < (MUSIC_FFT_SIZE / 2U); bi++)
                {
                    log_printf("%lu", (unsigned long)(uint32_t)s_mag[bi]);
                    if ((bi + 1U) % 16U == 0U)
                    {
                        log_printf("\r\n[FFT] mag=");
                    }
                    else
                    {
                        log_printf(" ");
                    }
                }
                log_printf("\r\n");

                log_printf("[FFT] bars %u %u %u %u %u %u %u %u\r\n",
                    s_bars[0], s_bars[1], s_bars[2], s_bars[3],
                    s_bars[4], s_bars[5], s_bars[6], s_bars[7]);
            }
        }
#endif
    }
    else
    {
        MusicFFT_DecayBars();
        if (s_silent_frames < MUSIC_FFT_NO_SIGNAL_FRAMES)
        {
            s_silent_frames++;
        }
    }
}

void MusicFFT_GetBars(uint8_t *bars, uint8_t count)
{
    uint8_t i;

    if (bars == NULL)
    {
        return;
    }

    for (i = 0U; i < count; i++)
    {
        bars[i] = (i < MUSIC_FFT_BAR_COUNT) ? s_bars[i] : 0U;
    }
}

uint8_t MusicFFT_HasSignal(void)
{
    return (s_silent_frames < MUSIC_FFT_NO_SIGNAL_FRAMES) ? 1U : 0U;
}

/* ------------------------------------------------------------------ */
/*  DMA transfer-complete callback — called from ISR context           */
/*  Switches to the other ping-pong buffer and restarts DMA.           */
/* ------------------------------------------------------------------ */
void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai)
{
    static uint8_t s_next_buf = 0U;

    if (hsai != &hsai_BlockA1)
    {
        return;
    }

    /* Flip to the other buffer for the next transfer. */
    s_next_buf ^= 1U;

    /* Signal the just-completed buffer as ready for FFT processing. */
    s_buf_ready_idx = (uint8_t)(s_next_buf ^ 1U);

    /* Restart DMA to fill the other buffer (NORMAL mode, one-shot). */
    (void)HAL_SAI_Receive_DMA(&hsai_BlockA1, (uint8_t *)s_dma_buf[s_next_buf], MUSIC_FFT_SIZE);
}
