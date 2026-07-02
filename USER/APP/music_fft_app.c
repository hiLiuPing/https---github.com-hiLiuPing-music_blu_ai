#include "music_fft_app.h"

#include "FreeRTOS.h"
#include "task.h"
#include "arm_math.h"
#include "log.h"
#include "sai.h"
#include "user_TasksInit.h"

#include <math.h>
#include <string.h>

#define MUSIC_FFT_HALF_WORDS      (MUSIC_FFT_DMA_WORDS / 2U)
#define MUSIC_FFT_FRAME_WORDS     2U
#define MUSIC_FFT_BAR_MAX         18U
#define MUSIC_FFT_PEAK_DECAY      0.96f
#define MUSIC_FFT_MIN_PEAK        1.0e-6f
#define MUSIC_FFT_ENERGY_EPS      1.0e-12f
#define MUSIC_FFT_LOG_GAIN        2048.0f
#define MUSIC_FFT_TWO_PI          6.28318530717958647692f

typedef struct
{
    uint16_t start_bin;
    uint16_t end_bin;
} MusicFFT_BinRange_t;

typedef struct
{
    uint8_t initialized;
    volatile uint8_t running;
    uint8_t data_valid;
    uint16_t sample_fill;
    uint8_t decimation_count;
    float peak_level;
    float decimation_sum;
    arm_rfft_fast_instance_f32 fft;
    uint32_t dma_buffer[MUSIC_FFT_DMA_WORDS];
    float window[MUSIC_FFT_SIZE];
    float time_domain[MUSIC_FFT_SIZE];
    float fft_output[MUSIC_FFT_SIZE];
    uint8_t bars[MUSIC_FFT_BAR_COUNT];
} MusicFFT_Runtime_t;

static MusicFFT_Runtime_t s_music_fft;

static const MusicFFT_BinRange_t s_music_fft_ranges[MUSIC_FFT_BAR_COUNT] = {
    {1U, 2U},
    {3U, 4U},
    {5U, 7U},
    {8U, 11U},
    {12U, 16U},
    {17U, 24U},
    {25U, 35U},
    {36U, 63U},
};

static void MusicFFT_ClearBars(void)
{
    taskENTER_CRITICAL();
    memset(s_music_fft.bars, 0, sizeof(s_music_fft.bars));
    taskEXIT_CRITICAL();
}

static float MusicFFT_WordToFloat(uint32_t raw_word)
{
    int32_t sample = (int32_t)(raw_word & 0x00FFFFFFUL);

    if ((sample & 0x00800000L) != 0L)
    {
        sample |= (int32_t)0xFF000000L;
    }

    return (float)sample / 8388608.0f;
}

static void MusicFFT_UpdateBars(const float raw_levels[MUSIC_FFT_BAR_COUNT], float frame_peak)
{
    uint8_t next_bars[MUSIC_FFT_BAR_COUNT];
    uint8_t i;
    float peak = s_music_fft.peak_level;

    if (frame_peak > peak)
    {
        peak = frame_peak;
    }
    else
    {
        peak *= MUSIC_FFT_PEAK_DECAY;
        if (frame_peak > peak)
        {
            peak = frame_peak;
        }
    }

    if (peak < MUSIC_FFT_MIN_PEAK)
    {
        peak = MUSIC_FFT_MIN_PEAK;
    }

    s_music_fft.peak_level = peak;

    for (i = 0U; i < MUSIC_FFT_BAR_COUNT; i++)
    {
        float normalized = raw_levels[i] / peak;
        uint8_t target;
        uint8_t current;
        uint8_t decay = 1U;

        if (normalized < 0.0f)
        {
            normalized = 0.0f;
        }
        if (normalized > 1.0f)
        {
            normalized = 1.0f;
        }

        target = (uint8_t)((normalized * (float)MUSIC_FFT_BAR_MAX) + 0.5f);
        if (target > MUSIC_FFT_BAR_MAX)
        {
            target = MUSIC_FFT_BAR_MAX;
        }

        current = s_music_fft.bars[i];
        if (target >= current)
        {
            next_bars[i] = target;
            continue;
        }

        if (current > 10U)
        {
            decay = 2U;
        }

        if (current > decay)
        {
            current = (uint8_t)(current - decay);
        }
        else
        {
            current = 0U;
        }

        if (target > current)
        {
            current = target;
        }

        next_bars[i] = current;
    }

    taskENTER_CRITICAL();
    memcpy(s_music_fft.bars, next_bars, sizeof(next_bars));
    taskEXIT_CRITICAL();
}

static void MusicFFT_PushSample(float sample)
{
    s_music_fft.decimation_sum += sample;
    s_music_fft.decimation_count++;

    if (s_music_fft.decimation_count < MUSIC_FFT_DECIMATION_FACTOR)
    {
        return;
    }

    s_music_fft.time_domain[s_music_fft.sample_fill++] =
        s_music_fft.decimation_sum / (float)MUSIC_FFT_DECIMATION_FACTOR;
    s_music_fft.decimation_sum = 0.0f;
    s_music_fft.decimation_count = 0U;
}

static void MusicFFT_RunFrame(void)
{
    float raw_levels[MUSIC_FFT_BAR_COUNT];
    float frame_peak = 0.0f;
    float mean = 0.0f;
    uint16_t i;

    for (i = 0U; i < MUSIC_FFT_SIZE; i++)
    {
        mean += s_music_fft.time_domain[i];
    }
    mean /= (float)MUSIC_FFT_SIZE;

    for (i = 0U; i < MUSIC_FFT_SIZE; i++)
    {
        s_music_fft.time_domain[i] = (s_music_fft.time_domain[i] - mean) * s_music_fft.window[i];
    }

    arm_rfft_fast_f32(&s_music_fft.fft, s_music_fft.time_domain, s_music_fft.fft_output, 0U);

    for (i = 0U; i < MUSIC_FFT_BAR_COUNT; i++)
    {
        const MusicFFT_BinRange_t *range = &s_music_fft_ranges[i];
        float energy = 0.0f;
        uint16_t bin;
        uint16_t count = (uint16_t)(range->end_bin - range->start_bin + 1U);

        for (bin = range->start_bin; bin <= range->end_bin; bin++)
        {
            float real = s_music_fft.fft_output[bin * 2U];
            float imag = s_music_fft.fft_output[(bin * 2U) + 1U];

            energy += (real * real) + (imag * imag);
        }

        raw_levels[i] = 20.0f * log10f(
            1.0f + (sqrtf((energy / (float)count) + MUSIC_FFT_ENERGY_EPS) * MUSIC_FFT_LOG_GAIN));
        if (raw_levels[i] > frame_peak)
        {
            frame_peak = raw_levels[i];
        }
    }

    MusicFFT_UpdateBars(raw_levels, frame_peak);
    s_music_fft.data_valid = 1U;
}

static void MusicFFT_NotifyTaskFromISR(uint32_t notify_mask)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (MusicFFTTaskHandle == NULL)
    {
        return;
    }

    xTaskNotifyFromISR(MusicFFTTaskHandle, notify_mask, eSetBits, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

void MusicFFT_AppInit(void)
{
    uint16_t i;
    arm_status status;

    memset(&s_music_fft, 0, sizeof(s_music_fft));
    status = arm_rfft_fast_init_f32(&s_music_fft.fft, MUSIC_FFT_SIZE);
    if (status != ARM_MATH_SUCCESS)
    {
        log_printf("[MusicFFT] FFT init failed: %d\r\n", (int)status);
        return;
    }

    for (i = 0U; i < MUSIC_FFT_SIZE; i++)
    {
        s_music_fft.window[i] = 0.5f - (0.5f * cosf((MUSIC_FFT_TWO_PI * (float)i) / (float)(MUSIC_FFT_SIZE - 1U)));
    }

    s_music_fft.peak_level = MUSIC_FFT_MIN_PEAK;
    s_music_fft.initialized = 1U;
}

HAL_StatusTypeDef MusicFFT_Start(void)
{
    HAL_StatusTypeDef status;

    if (!s_music_fft.initialized)
    {
        return HAL_ERROR;
    }

    if (s_music_fft.running)
    {
        return HAL_OK;
    }

    MusicFFT_Reset();

    status = HAL_SAI_Receive_DMA(&hsai_BlockA1, (uint8_t *)s_music_fft.dma_buffer, MUSIC_FFT_DMA_WORDS);
    if (status == HAL_OK)
    {
        s_music_fft.running = 1U;
    }
    else
    {
        log_printf("[MusicFFT] SAI DMA start failed: %ld\r\n", (long)status);
    }

    return status;
}

void MusicFFT_Stop(void)
{
    if (s_music_fft.running)
    {
        s_music_fft.running = 0U;
        (void)HAL_SAI_DMAStop(&hsai_BlockA1);
    }

    MusicFFT_Reset();
}

void MusicFFT_Reset(void)
{
    memset(s_music_fft.dma_buffer, 0, sizeof(s_music_fft.dma_buffer));
    memset(s_music_fft.time_domain, 0, sizeof(s_music_fft.time_domain));
    memset(s_music_fft.fft_output, 0, sizeof(s_music_fft.fft_output));
    s_music_fft.sample_fill = 0U;
    s_music_fft.decimation_count = 0U;
    s_music_fft.decimation_sum = 0.0f;
    s_music_fft.data_valid = 0U;
    s_music_fft.peak_level = MUSIC_FFT_MIN_PEAK;
    MusicFFT_ClearBars();
}

void MusicFFT_GetBars(uint8_t out[MUSIC_FFT_BAR_COUNT])
{
    if (out == NULL)
    {
        return;
    }

    taskENTER_CRITICAL();
    memcpy(out, s_music_fft.bars, sizeof(s_music_fft.bars));
    taskEXIT_CRITICAL();
}

uint8_t MusicFFT_IsRunning(void)
{
    return s_music_fft.running;
}

void MusicFFT_ProcessDmaHalf(uint8_t half_index)
{
    uint16_t src_start;
    uint16_t src_end;
    uint16_t src;

    if (!s_music_fft.running || half_index > 1U)
    {
        return;
    }

    src_start = (uint16_t)(half_index * MUSIC_FFT_HALF_WORDS);
    src_end = (uint16_t)(src_start + MUSIC_FFT_HALF_WORDS);

    for (src = src_start; (uint16_t)(src + MUSIC_FFT_FRAME_WORDS) <= src_end; src += MUSIC_FFT_FRAME_WORDS)
    {
        uint16_t left_index = (uint16_t)(src + MUSIC_FFT_LEFT_SLOT_INDEX);
        float left_sample = MusicFFT_WordToFloat(s_music_fft.dma_buffer[left_index]);

        MusicFFT_PushSample(left_sample);

        if (s_music_fft.sample_fill >= MUSIC_FFT_SIZE)
        {
            MusicFFT_RunFrame();
            s_music_fft.sample_fill = 0U;
        }
    }
}

void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai == &hsai_BlockA1)
    {
        MusicFFT_NotifyTaskFromISR(MUSIC_FFT_NOTIFY_HALF);
    }
}

void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai == &hsai_BlockA1)
    {
        MusicFFT_NotifyTaskFromISR(MUSIC_FFT_NOTIFY_FULL);
    }
}

void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai == &hsai_BlockA1)
    {
        s_music_fft.running = 0U;
        memset(s_music_fft.dma_buffer, 0, sizeof(s_music_fft.dma_buffer));
        memset(s_music_fft.time_domain, 0, sizeof(s_music_fft.time_domain));
        memset(s_music_fft.fft_output, 0, sizeof(s_music_fft.fft_output));
        memset(s_music_fft.bars, 0, sizeof(s_music_fft.bars));
        s_music_fft.sample_fill = 0U;
        s_music_fft.decimation_count = 0U;
        s_music_fft.decimation_sum = 0.0f;
        s_music_fft.data_valid = 0U;
        s_music_fft.peak_level = MUSIC_FFT_MIN_PEAK;
    }
}
