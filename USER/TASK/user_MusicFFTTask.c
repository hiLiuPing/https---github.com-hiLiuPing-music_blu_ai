#include "user_MusicFFTTask.h"

#include "music_fft_app.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>

void MusicFFTTask(void *pvParameters)
{
    uint32_t notify_value;

    (void)pvParameters;

    for (;;)
    {
        notify_value = 0U;
        if (xTaskNotifyWait(0U, UINT32_MAX, &notify_value, portMAX_DELAY) != pdPASS)
        {
            continue;
        }

        if ((notify_value & MUSIC_FFT_NOTIFY_HALF) != 0U)
        {
            MusicFFT_ProcessDmaHalf(0U);
        }

        if ((notify_value & MUSIC_FFT_NOTIFY_FULL) != 0U)
        {
            MusicFFT_ProcessDmaHalf(1U);
        }
    }
}
