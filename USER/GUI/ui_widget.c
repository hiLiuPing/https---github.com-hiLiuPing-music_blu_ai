#include "ui_widget.h"
#include "oled_weather_font.h"

/*
 * Widget 资源表与基础绘制辅助。
 * 这里不决定页面逻辑，只提供页面可复用的局部绘制能力。
 */

const UI_DrawFunc_t g_ui_region0_lib[] = {
    // UI_Widget_DrawHomeDefaultLeft,
        UI_Widget_DrawRegionIcon,
    UI_Widget_DrawRegionWeather,
    UI_Widget_DrawRegionTemp,
    // UI_Widget_DrawRegionPower,
    UI_Widget_DrawRegionHighTemp,
    UI_Widget_DrawRegionLowTemp,
    UI_Widget_DrawRegionPM25,
    UI_Widget_DrawRegionHumidity,
    UI_Widget_DrawRegionAir,

};

const UI_DrawFunc_t g_ui_region1_lib[] = {
    // UI_Widget_DrawHomeDefaultMiddle,
        UI_Widget_DrawHomeClockMid,
    UI_Widget_DrawRegionMusic,
    // UI_Widget_DrawRegionTempHumid,
    // UI_Widget_DrawRegionWeatherPM,
    UI_Widget_DrawRegionIcon1_2,

};

const UI_DrawFunc_t g_ui_region2_lib[] = {
    // UI_Widget_DrawHomeDefaultRight,
        UI_Widget_DrawHomeClockSec,
    UI_Widget_DrawRegionBattery,
    // UI_Widget_DrawRegionSystem,
    UI_Widget_DrawRegionTempNow,
    // UI_Widget_DrawRegionHighTemp,
    // UI_Widget_DrawRegionLowTemp,
    // UI_Widget_DrawRegionPM25,
    UI_Widget_DrawRegionHumidityNow,
    UI_Widget_DrawRegionIcon3,

};

const UI_DrawFunc_t * const g_ui_region_libs[OLED_UI_REGION_COUNT] = {
    g_ui_region0_lib,
    g_ui_region1_lib,
    g_ui_region2_lib,
};

const uint8_t g_ui_region_lib_counts[OLED_UI_REGION_COUNT] = {
    (uint8_t)(sizeof(g_ui_region0_lib) / sizeof(g_ui_region0_lib[0])),
    (uint8_t)(sizeof(g_ui_region1_lib) / sizeof(g_ui_region1_lib[0])),
    (uint8_t)(sizeof(g_ui_region2_lib) / sizeof(g_ui_region2_lib[0])),
};

const UI_DrawFunc_t g_ui_home_default_draws[OLED_UI_REGION_COUNT] = {
    UI_Widget_DrawRegionIcon,
    UI_Widget_DrawHomeClockMid,
    UI_Widget_DrawHomeClockSec,
};

static uint16_t UI_Widget_DecodeUtf8Char(const char **cursor)
{
    const uint8_t *src = (const uint8_t *)(*cursor);
    uint16_t codepoint;

    if (src == NULL || src[0] == '\0')
    {
        return 0U;
    }

    if (src[0] < 0x80U)
    {
        *cursor += 1;
        return src[0];
    }

    if ((src[0] & 0xE0U) == 0xC0U && src[1] != '\0')
    {
        codepoint = (uint16_t)(((uint16_t)(src[0] & 0x1FU) << 6) | (uint16_t)(src[1] & 0x3FU));
        *cursor += 2;
        return codepoint;
    }

    if ((src[0] & 0xF0U) == 0xE0U && src[1] != '\0' && src[2] != '\0')
    {
        codepoint = (uint16_t)(((uint16_t)(src[0] & 0x0FU) << 12) |
                               ((uint16_t)(src[1] & 0x3FU) << 6) |
                               (uint16_t)(src[2] & 0x3FU));
        *cursor += 3;
        return codepoint;
    }

    *cursor += 1;
    return (uint16_t)'?';
}

static uint8_t UI_Widget_WeatherTextWidth(const char *str)
{
    const char *cursor = str;
    uint16_t width = 0U;

    while (cursor != NULL && *cursor != '\0')
    {
        uint16_t codepoint = UI_Widget_DecodeUtf8Char(&cursor);
        uint8_t glyph_w = OLED_WeatherFontWidth(codepoint);

        if (glyph_w > 0U)
        {
            width = (uint16_t)(width + glyph_w + 1U);
        }
    }

    if (width > 0U)
    {
        width--;
    }

    return (uint8_t)((width > 255U) ? 255U : width);
}

void UI_Widget_DrawWeatherStr(u8g2_t *u8g2, int16_t x, int16_t y, const char *str)
{
    const char *cursor = str;

    if (str == NULL)
    {
        return;
    }

    while (cursor != NULL && *cursor != '\0')
    {
        uint8_t page;
        uint8_t col;
        uint16_t codepoint = UI_Widget_DecodeUtf8Char(&cursor);
        const uint8_t *glyph = OLED_WeatherFontLookup(codepoint);
        uint8_t glyph_w = OLED_WeatherFontWidth(codepoint);

        if (glyph == NULL || glyph_w == 0U)
        {
            continue;
        }

        for (page = 0U; page < OLED_WEATHER_FONT_PAGE_COUNT; page++)
        {
            for (col = 0U; col < glyph_w; col++)
            {
                uint8_t data = glyph[(page * OLED_WEATHER_FONT_WIDTH) + col];
                uint8_t bit;

                if (data == 0U)
                {
                    continue;
                }

                for (bit = 0U; bit < 8U; bit++)
                {
                    uint8_t row = (uint8_t)(page * 8U + bit);

                    if (row >= OLED_WEATHER_FONT_HEIGHT)
                    {
                        break;
                    }

                    if ((data & (uint8_t)(1U << bit)) != 0U)
                    {
                        u8g2_DrawPixel(u8g2,
                                       x + col,
                                       y - OLED_WEATHER_FONT_HEIGHT + 1 + row);
                    }
                }
            }
        }

        x += glyph_w + 1;
    }
}

void UI_Widget_DrawCenteredWeatherStr(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, const char *str)
{
    int16_t str_w;

    if (str == NULL)
    {
        return;
    }

    str_w = (int16_t)UI_Widget_WeatherTextWidth(str);
    UI_Widget_DrawWeatherStr(u8g2, x + ((int16_t)w - str_w) / 2, y, str);
}

/**
 * @brief  电池图标和进度条绘制
 */
void UI_Widget_DrawStatusBattery(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t pct, uint8_t charging)
{
    uint8_t fill;
    uint8_t body_w = 20U;
    uint8_t body_h = 10U;
    uint8_t cap_w = 2U;
    uint8_t cap_h = 4U;

    if (pct > 100U)
    {
        pct = 100U;
    }

    fill = (uint8_t)(((body_w - 4U) * pct) / 100U);

    u8g2_DrawFrame(u8g2, x, y, body_w, body_h);
    u8g2_DrawBox(u8g2, x + body_w, y + 3, cap_w, cap_h);

    if (charging)
    {
        uint32_t period_ms = 3000U;
        uint32_t elapsed = xTaskGetTickCount() % pdMS_TO_TICKS(period_ms);
        uint16_t fill_max = (uint16_t)(body_w - 4U) * pct / 100U;
        fill = (uint8_t)((uint32_t)fill_max * elapsed / pdMS_TO_TICKS(period_ms));
    }

    u8g2_DrawBox(u8g2, x + 2, y + 2, fill, body_h - 4U);
}

/**
 * @brief  小型频谱动画
 */
void UI_Widget_DrawTinySpectrum(u8g2_t *u8g2, int16_t x, int16_t y)
{
    uint8_t i;
    uint16_t phase = (uint16_t)((xTaskGetTickCount() / pdMS_TO_TICKS(16U)) & 0xFFU);

    for (i = 0; i < 6U; i++)
    {
        uint8_t h = (uint8_t)(3U + ((phase + (i * 3U)) % 10U));
        u8g2_DrawBox(u8g2, x + (i * 4), y - h, 2, h);
    }
}

/**
 * @brief  指定宽度内居中显示字符串
 */
void UI_Widget_DrawCenteredStr(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, const char *str)
{
    int16_t str_w;

    if (str == NULL)
    {
        return;
    }

    str_w = (int16_t)u8g2_GetStrWidth(u8g2, str);
    u8g2_DrawStr(u8g2, x + ((int16_t)w - str_w) / 2, y, str);
}

void UI_Widget_DrawCenteredStrUTF8(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, const char *str)
{
    int16_t str_w;

    if (str == NULL)
    {
        return;
    }

    str_w = (int16_t)u8g2_GetUTF8Width(u8g2, str);
    u8g2_DrawUTF8(u8g2, x + ((int16_t)w - str_w) / 2, y, str);
}
