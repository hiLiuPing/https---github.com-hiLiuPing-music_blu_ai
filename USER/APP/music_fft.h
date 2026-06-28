#ifndef __MUSIC_FFT_H__
#define __MUSIC_FFT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

#define MUSIC_FFT_SIZE        256U
#define MUSIC_FFT_BAR_COUNT   8U
#define MUSIC_FFT_BAR_MAX     31U

HAL_StatusTypeDef MusicFFT_Init(void);
void MusicFFT_Start(void);
void MusicFFT_Stop(void);
void MusicFFT_Process(void);
void MusicFFT_GetBars(uint8_t *bars, uint8_t count);
uint8_t MusicFFT_HasSignal(void);

void MusicFFT_OnDmaHalf(void);
void MusicFFT_OnDmaFull(void);

#ifdef __cplusplus
}
#endif

#endif
