#ifndef __MUSIC_FFT_APP_H__
#define __MUSIC_FFT_APP_H__

#include "stm32l4xx_hal.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MUSIC_FFT_SIZE            256U
#define MUSIC_FFT_BAR_COUNT       8U
#define MUSIC_FFT_DMA_WORDS       512U
#define MUSIC_FFT_SRC_RATE_HZ     96000U
#define MUSIC_FFT_SLOT_BITS       32U
#define MUSIC_FFT_SAMPLE_BITS     24U
#define MUSIC_FFT_LEFT_SLOT_INDEX 0U
#define MUSIC_FFT_DECIMATION_FACTOR 8U
#define MUSIC_FFT_VIS_RATE_HZ     (MUSIC_FFT_SRC_RATE_HZ / MUSIC_FFT_DECIMATION_FACTOR)
#define MUSIC_FFT_NOTIFY_HALF     (1UL << 0)
#define MUSIC_FFT_NOTIFY_FULL     (1UL << 1)

void MusicFFT_AppInit(void);
HAL_StatusTypeDef MusicFFT_Start(void);
void MusicFFT_Stop(void);
void MusicFFT_Reset(void);
void MusicFFT_GetBars(uint8_t out[MUSIC_FFT_BAR_COUNT]);
uint8_t MusicFFT_IsRunning(void);
void MusicFFT_ProcessDmaHalf(uint8_t half_index);

#ifdef __cplusplus
}
#endif

#endif
