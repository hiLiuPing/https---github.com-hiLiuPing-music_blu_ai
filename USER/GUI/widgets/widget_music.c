#include "../ui_widget.h"
#include "music_fft.h"

static uint8_t UI_Widget_MusicTriWave(uint8_t phase, uint8_t period)
{
    uint8_t half_period;

    if (period < 2U)
    {
        return 0U;
    }

    phase = (uint8_t)(phase % period);
    half_period = (uint8_t)(period / 2U);

    if (phase >= half_period)
    {
        return (uint8_t)(period - phase - 1U);
    }

    return phase;
}

static void UI_Widget_DrawMusicPauseIcon(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    uint8_t ring_d = 20U;
    uint8_t icon_h = 12U;
    uint8_t wave = UI_Widget_MusicTriWave((uint8_t)g_ui_spectrum_tick, 16U);
    int16_t bounce = ((int16_t)wave / 2) - 2;
    int16_t ring_x = x + (((int16_t)w - ring_d) / 2);
    int16_t ring_y = y + (((int16_t)h - ring_d) / 2) + bounce;
    int16_t icon_x = ring_x + 4;
    int16_t icon_y = ring_y + 4;

    u8g2_DrawCircle(u8g2, ring_x + 10, ring_y + 10, 10, U8G2_DRAW_ALL);
    u8g2_DrawRBox(u8g2, icon_x, icon_y, 4, icon_h, 1);
    u8g2_DrawRBox(u8g2, icon_x + 8, icon_y, 4, icon_h, 1);
}

static void UI_Widget_DrawMusicSpectrum(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    uint8_t bars[MUSIC_FFT_BAR_COUNT];
    uint8_t i;
    uint8_t bar_w = 4U;
    uint8_t bar_gap = 3U;
    uint8_t pair_count = 4U;
    uint8_t total_bars = (uint8_t)(pair_count * 2U);
    uint8_t total_w = (uint8_t)((total_bars * bar_w) + ((total_bars - 1U) * bar_gap));
    int16_t start_x = x + (((int16_t)w - total_w) / 2);
    int16_t baseline_y = y + (int16_t)h - 4;
    uint8_t max_h = (h > 8U) ? (uint8_t)(h - 8U) : h;

    MusicFFT_GetBars(bars, MUSIC_FFT_BAR_COUNT);

    for (i = 0; i < pair_count; i++)
    {
        uint8_t left_h = (uint8_t)((bars[i] * max_h) / MUSIC_FFT_BAR_MAX);
        uint8_t right_h = (uint8_t)((bars[MUSIC_FFT_BAR_COUNT - 1U - i] * max_h) / MUSIC_FFT_BAR_MAX);
        int16_t left_x = start_x + (int16_t)(i * (bar_w + bar_gap));
        int16_t right_x = start_x + (int16_t)((total_bars - 1U - i) * (bar_w + bar_gap));

        if (left_h < 1U)
        {
            left_h = 1U;
        }
        if (right_h < 1U)
        {
            right_h = 1U;
        }

        u8g2_DrawRBox(u8g2, left_x, baseline_y - left_h, bar_w, left_h, 1);
        u8g2_DrawRBox(u8g2, right_x, baseline_y - right_h, bar_w, right_h, 1);
    }
}

void UI_Widget_DrawRegionMusic(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    if (g_music_ble_state.music_played)
    {
        UI_Widget_DrawMusicSpectrum(u8g2, x, y, w, h);
    }
    else
    {
        UI_Widget_DrawMusicPauseIcon(u8g2, x, y, w, h);
    }

    g_ui_spectrum_tick++;
}
