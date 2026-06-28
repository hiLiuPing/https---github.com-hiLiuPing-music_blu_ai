#include "music_fft.h"

#include "music_app.h"
#include "sai.h"

#include <math.h>
#include <string.h>

#if !defined(ARM_MATH_CM4)
#define ARM_MATH_CM4
#endif
#include "arm_math.h"

#define MUSIC_FFT_CHANNELS          2U
#define MUSIC_FFT_DMA_HALF_WORDS    (MUSIC_FFT_SIZE * MUSIC_FFT_CHANNELS)
#define MUSIC_FFT_DMA_WORDS         (MUSIC_FFT_DMA_HALF_WORDS * 2U)
#define MUSIC_FFT_PI                3.14159265358979323846f
#define MUSIC_FFT_NO_SIGNAL_FRAMES  18U

static uint32_t s_dma_buf[MUSIC_FFT_DMA_WORDS];
static float s_fft_in[MUSIC_FFT_SIZE];
static float s_window[MUSIC_FFT_SIZE];
static float s_mag[MUSIC_FFT_SIZE / 2U];
static uint8_t s_bars[MUSIC_FFT_BAR_COUNT];
static uint8_t s_running;
static uint8_t s_initialized;
static volatile uint8_t s_pending_half;
static volatile uint8_t s_pending_full;
static uint8_t s_silent_frames;
static arm_rfft_fast_instance_f32 s_rfft;
static float s_fft_out[MUSIC_FFT_SIZE];
static const uint16_t s_bar_edges[MUSIC_FFT_BAR_COUNT + 1U] = {
    2U, 4U, 7U, 11U, 17U, 26U, 39U, 59U, (MUSIC_FFT_SIZE / 2U)
};

static int16_t MusicFFT_DecodeSample(uint32_t sample_word)
{
    return (int16_t)(sample_word & 0xFFFFU);
}

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
        int16_t left = MusicFFT_DecodeSample(src[(i * MUSIC_FFT_CHANNELS)]);
        int16_t right = MusicFFT_DecodeSample(src[(i * MUSIC_FFT_CHANNELS) + 1U]);
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
        uint8_t level = MusicFFT_LevelToBar(avg / 180.0f);

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

    for (i = 0U; i < MUSIC_FFT_SIZE; i++)
    {
        s_window[i] = 0.5f - (0.5f * cosf((2.0f * MUSIC_FFT_PI * (float)i) / (float)(MUSIC_FFT_SIZE - 1U)));
    }

    if (arm_rfft_fast_init_f32(&s_rfft, MUSIC_FFT_SIZE) != ARM_MATH_SUCCESS)
    {
        return HAL_ERROR;
    }

    status = HAL_SAI_Receive_DMA(&hsai_BlockA1, (uint8_t *)s_dma_buf, (uint16_t)MUSIC_FFT_DMA_WORDS);
    if (status == HAL_OK)
    {
        s_initialized = 1U;
        s_running = 1U;
    }

    return status;
}

void MusicFFT_Start(void)
{
    s_pending_half = 0U;
    s_pending_full = 0U;
    s_running = 1U;
}

void MusicFFT_Stop(void)
{
    s_running = 0U;
    s_pending_half = 0U;
    s_pending_full = 0U;
    MusicFFT_ResetBars();
}

void MusicFFT_Process(void)
{
    const uint32_t *src = NULL;

    if (!s_initialized)
    {
        return;
    }

    if (!g_music_ble_state.music_played)
    {
        MusicFFT_Stop();
        return;
    }

    if (!s_running)
    {
        MusicFFT_Start();
    }

    if (s_pending_full)
    {
        src = &s_dma_buf[MUSIC_FFT_DMA_HALF_WORDS];
        s_pending_full = 0U;
    }
    else if (s_pending_half)
    {
        src = &s_dma_buf[0];
        s_pending_half = 0U;
    }
    else
    {
        MusicFFT_DecayBars();
        if (s_silent_frames < MUSIC_FFT_NO_SIGNAL_FRAMES)
        {
            s_silent_frames++;
        }
        return;
    }

    MusicFFT_BuildInput(src);
    MusicFFT_RunFft();
    MusicFFT_UpdateBars();
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

void MusicFFT_OnDmaHalf(void)
{
    s_pending_half = 1U;
}

void MusicFFT_OnDmaFull(void)
{
    s_pending_full = 1U;
}

void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai == &hsai_BlockA1)
    {
        MusicFFT_OnDmaHalf();
    }
}

void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai == &hsai_BlockA1)
    {
        MusicFFT_OnDmaFull();
    }
}
