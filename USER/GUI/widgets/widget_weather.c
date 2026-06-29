#include "../ui_widget.h"
#include "../icons.h"
#include "sensors_app.h"

#define UI_WIDGET_WEATHER_FRAME_MS  120U

static int UI_Widget_RoundFloatToInt(float value)
{
    return (value >= 0.0f) ? (int)(value + 0.5f) : (int)(value - 0.5f);
}

static int UI_Widget_LocalHumidityPercent(void)
{
    int hum = UI_Widget_RoundFloatToInt(g_sensors_environment.hum);

    if (hum < 0)
    {
        hum = 0;
    }
    else if (hum > 100)
    {
        hum = 100;
    }

    return hum;
}

static void UI_Widget_FormatTempTenths(char *buf, uint8_t size, float value)
{
    memset(buf, 0, size); // 函数内强制清空缓冲区
    int tenths = (value >= 0.0f) ? (int)(value * 10.0f + 0.5f) : (int)(value * 10.0f - 0.5f);
    int abs_tenths = (tenths < 0) ? -tenths : tenths;

    snprintf(buf, size, "%s%d.%dC", (tenths < 0) ? "-" : "", abs_tenths / 10, abs_tenths % 10);
}

typedef enum {
    UI_WEATHER_ICON_UNKNOWN = 0U,
    UI_WEATHER_ICON_CLEAR,
    UI_WEATHER_ICON_MOON_PHASE,
    UI_WEATHER_ICON_PARTLY,
    UI_WEATHER_ICON_CLOUDY,
    UI_WEATHER_ICON_RAIN,
    UI_WEATHER_ICON_SNOW,
    UI_WEATHER_ICON_THUNDER,
    UI_WEATHER_ICON_FOG,
    UI_WEATHER_ICON_DUST,
    UI_WEATHER_ICON_WIND,
    UI_WEATHER_ICON_HOT,
    UI_WEATHER_ICON_COLD,
} UI_WeatherIconKind_t;

typedef enum {
    UI_WEATHER_MOTION_MAIN = 0U,
    UI_WEATHER_MOTION_FUTURE_SINGLE,
    UI_WEATHER_MOTION_FUTURE_PAIR,
} UI_WeatherMotion_t;

typedef struct {
    UI_WeatherIconKind_t kind;
    uint8_t level;
    uint8_t night;
    uint8_t mixed;
    uint8_t severe;
} UI_WeatherStyle_t;
/* ── 新增天气详情 Widget ── */

static const uint8_t *UI_Widget_IconPtr(int icon_id)
{
    switch (icon_id)
    {
    case 100: return U8_IMG_100;
    case 101: return U8_IMG_101;
    case 102: return U8_IMG_102;
    case 103: return U8_IMG_103;
    case 104: return U8_IMG_104;
    case 150: return U8_IMG_150;
    case 153: return U8_IMG_153;
    case 154: return U8_IMG_154;
    case 300: return U8_IMG_300;
    case 301: return U8_IMG_301;
    case 302: return U8_IMG_302;
    case 303: return U8_IMG_303;
    case 304: return U8_IMG_304;
    case 305: return U8_IMG_305;
    case 306: return U8_IMG_306;
    case 307: return U8_IMG_307;
    case 308: return U8_IMG_308;
    case 309: return U8_IMG_309;
    case 310: return U8_IMG_310;
    case 311: return U8_IMG_311;
    case 312: return U8_IMG_312;
    case 313: return U8_IMG_313;
    case 314: return U8_IMG_314;
    case 315: return U8_IMG_315;
    case 316: return U8_IMG_316;
    case 317: return U8_IMG_317;
    case 318: return U8_IMG_318;
    case 350: return U8_IMG_350;
    case 351: return U8_IMG_351;
    case 399: return U8_IMG_399;
    case 400: return U8_IMG_400;
    case 401: return U8_IMG_401;
    case 402: return U8_IMG_402;
    case 403: return U8_IMG_403;
    case 404: return U8_IMG_404;
    case 405: return U8_IMG_405;
    case 406: return U8_IMG_406;
    case 407: return U8_IMG_407;
    case 408: return U8_IMG_408;
    case 409: return U8_IMG_409;
    case 410: return U8_IMG_410;
    case 456: return U8_IMG_456;
    case 457: return U8_IMG_457;
    case 499: return U8_IMG_499;
    case 500: return U8_IMG_500;
    case 501: return U8_IMG_501;
    case 502: return U8_IMG_502;
    case 503: return U8_IMG_503;
    case 504: return U8_IMG_504;
    case 507: return U8_IMG_507;
    case 508: return U8_IMG_508;
    case 509: return U8_IMG_509;
    case 510: return U8_IMG_510;
    case 511: return U8_IMG_511;
    case 512: return U8_IMG_512;
    case 513: return U8_IMG_513;
    case 514: return U8_IMG_514;
    case 515: return U8_IMG_515;
    case 900: return U8_IMG_900;
    case 901: return U8_IMG_901;
    case 999: return U8_IMG_999;
    default:  return NULL;
    }
}

static UI_WeatherStyle_t UI_Widget_WeatherResolve(int icon_id)
{
    UI_WeatherStyle_t style;

    memset(&style, 0, sizeof(style));
    style.kind = UI_WEATHER_ICON_UNKNOWN;

    if (icon_id >= 200 && icon_id <= 213)
    {
        style.kind = UI_WEATHER_ICON_WIND;
        style.level = (icon_id >= 208) ? 3U : ((icon_id >= 204) ? 2U : 1U);
        return style;
    }

    switch (icon_id)
    {
    case 100:
        style.kind = UI_WEATHER_ICON_CLEAR;
        return style;

    case 150:
        style.kind = UI_WEATHER_ICON_CLEAR;
        style.night = 1U;
        return style;

    case 101:
    case 102:
    case 103:
        style.kind = UI_WEATHER_ICON_PARTLY;
        style.level = (uint8_t)(icon_id - 101);
        return style;

    case 151:
    case 152:
    case 153:
        style.kind = UI_WEATHER_ICON_PARTLY;
        style.night = 1U;
        style.level = (uint8_t)(icon_id - 151);
        return style;

    case 154:
        style.kind = UI_WEATHER_ICON_PARTLY;
        style.night = 1U;
        style.level = 2U;
        return style;

    case 104:
        style.kind = UI_WEATHER_ICON_CLOUDY;
        style.level = 3U;
        return style;

    case 300:
    case 350:
        style.kind = UI_WEATHER_ICON_RAIN;
        style.level = 0U;
        return style;

    case 301:
    case 351:
        style.kind = UI_WEATHER_ICON_RAIN;
        style.level = 1U;
        return style;

    case 302:
        style.kind = UI_WEATHER_ICON_THUNDER;
        style.level = 1U;
        return style;

    case 303:
        style.kind = UI_WEATHER_ICON_THUNDER;
        style.level = 2U;
        style.severe = 1U;
        return style;

    case 304:
        style.kind = UI_WEATHER_ICON_THUNDER;
        style.level = 3U;
        style.severe = 1U;
        style.mixed = 1U;
        return style;

    case 305:
        style.kind = UI_WEATHER_ICON_RAIN;
        style.level = 1U;
        return style;

    case 306:
        style.kind = UI_WEATHER_ICON_RAIN;
        style.level = 2U;
        return style;

    case 307:
    case 456:
        style.kind = UI_WEATHER_ICON_RAIN;
        style.level = 3U;
        return style;

    case 308:
    case 309:
    case 310:
    case 399:
    case 457:
        style.kind = UI_WEATHER_ICON_RAIN;
        style.level = 4U;
        style.severe = 1U;
        return style;

    case 311:
        style.kind = UI_WEATHER_ICON_RAIN;
        style.level = 1U;
        style.mixed = 1U;
        return style;

    case 312:
        style.kind = UI_WEATHER_ICON_RAIN;
        style.level = 2U;
        style.mixed = 1U;
        return style;

    case 313:
        style.kind = UI_WEATHER_ICON_RAIN;
        style.level = 3U;
        style.mixed = 1U;
        style.severe = 1U;
        return style;

    case 314:
    case 400:
    case 406:
    case 407:
        style.kind = UI_WEATHER_ICON_SNOW;
        style.level = (icon_id == 407) ? 0U : 1U;
        return style;

    case 315:
    case 401:
        style.kind = UI_WEATHER_ICON_SNOW;
        style.level = 2U;
        return style;

    case 316:
    case 402:
    case 408:
        style.kind = UI_WEATHER_ICON_SNOW;
        style.level = 3U;
        return style;

    case 317:
    case 403:
    case 409:
    case 410:
        style.kind = UI_WEATHER_ICON_SNOW;
        style.level = 4U;
        style.severe = 1U;
        return style;

    case 318:
    case 404:
    case 405:
        style.kind = UI_WEATHER_ICON_SNOW;
        style.level = 2U;
        style.mixed = 1U;
        return style;

    case 500:
    case 514:
        style.kind = UI_WEATHER_ICON_FOG;
        style.level = 1U;
        return style;

    case 509:
        style.kind = UI_WEATHER_ICON_FOG;
        style.level = 3U;
        return style;

    case 510:
        style.kind = UI_WEATHER_ICON_FOG;
        style.level = 4U;
        style.severe = 1U;
        return style;

    case 501:
    case 502:
    case 511:
        style.kind = UI_WEATHER_ICON_DUST;
        style.level = 1U;
        return style;

    case 503:
    case 504:
    case 515:
        style.kind = UI_WEATHER_ICON_DUST;
        style.level = 2U;
        return style;

    case 507:
        style.kind = UI_WEATHER_ICON_DUST;
        style.level = 3U;
        return style;

    case 508:
    case 512:
    case 513:
        style.kind = UI_WEATHER_ICON_DUST;
        style.level = 4U;
        style.severe = 1U;
        return style;

    case 900:
        style.kind = UI_WEATHER_ICON_HOT;
        style.level = 2U;
        return style;

    case 901:
        style.kind = UI_WEATHER_ICON_COLD;
        style.level = 2U;
        return style;

    case 800:
    case 801:
    case 802:
    case 803:
    case 804:
    case 805:
    case 806:
    case 807:
        style.kind = UI_WEATHER_ICON_MOON_PHASE;
        style.level = (uint8_t)(icon_id - 800);
        return style;

    default:
        return style;
    }
}

static uint8_t UI_Widget_WeatherAnimPhaseBase(void)
{
    TickType_t now = xTaskGetTickCount();
    TickType_t frame_ticks = pdMS_TO_TICKS(UI_WIDGET_WEATHER_FRAME_MS);

    if (frame_ticks == 0U)
    {
        frame_ticks = 1U;
    }

    return (uint8_t)(((uint32_t)(now / frame_ticks)) & 0x07U);
}

static uint8_t UI_Widget_WeatherPhaseWithOffset(uint8_t phase, uint8_t offset)
{
    return (uint8_t)((phase + offset) & 0x07U);
}

static uint8_t UI_Widget_WeatherReduceForPair(uint8_t value, UI_WeatherMotion_t motion, uint8_t floor)
{
    if (motion == UI_WEATHER_MOTION_FUTURE_PAIR && value > floor)
    {
        return (uint8_t)(value - 1U);
    }

    return value;
}

static void UI_Widget_DrawWeatherCloudBase(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t phase, uint8_t cover)
{
    int16_t sway = (int16_t)((phase & 0x01U) ? 1 : 0);
    int16_t ox = x + sway;

    u8g2_DrawDisc(u8g2, ox + 10, y + 14, 5, U8G2_DRAW_ALL);
    u8g2_DrawDisc(u8g2, ox + 18, y + 11, 7, U8G2_DRAW_ALL);
    u8g2_DrawDisc(u8g2, ox + 25, y + 14, 5, U8G2_DRAW_ALL);
    u8g2_DrawBox(u8g2, ox + 7, y + 14, 20, 7);

    if (cover >= 2U)
    {
        u8g2_DrawDisc(u8g2, ox + 7, y + 16, 3, U8G2_DRAW_ALL);
        u8g2_DrawDisc(u8g2, ox + 22, y + 16, 3, U8G2_DRAW_ALL);
        u8g2_DrawBox(u8g2, ox + 6, y + 17, 22, 4);
    }

    if (cover >= 3U)
    {
        u8g2_DrawDisc(u8g2, ox + 15, y + 18, 4, U8G2_DRAW_ALL);
        u8g2_DrawBox(u8g2, ox + 11, y + 18, 12, 4);
    }
}

static void UI_Widget_DrawWeatherSun(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t phase, uint8_t hot)
{
    static const int8_t ray_x[8][4] = {
        { 0,  8,  0, -8},
        { 4,  7, -4, -7},
        { 7,  4, -7, -4},
        { 8,  0, -8,  0},
        { 7, -4, -7,  4},
        { 4, -7, -4,  7},
        { 0, -8,  0,  8},
        {-4, -7,  4,  7},
    };
    static const int8_t ray_y[8][4] = {
        {-8,  0,  8,  0},
        {-7,  4,  7, -4},
        {-4,  7,  4, -7},
        { 0,  8,  0, -8},
        { 4,  7, -4, -7},
        { 7,  4, -7, -4},
        { 8,  0, -8,  0},
        { 7, -4, -7,  4},
    };
    int16_t cx = x + 18;
    int16_t cy = y + 11;
    uint8_t i;

    u8g2_DrawDisc(u8g2, cx, cy, 5, U8G2_DRAW_ALL);
    u8g2_DrawCircle(u8g2, cx, cy, 7, U8G2_DRAW_ALL);

    for (i = 0; i < 4U; i++)
    {
        int16_t x1 = cx + ray_x[phase][i];
        int16_t y1 = cy + ray_y[phase][i];
        int16_t x2 = x1 + ((ray_x[phase][i] > 0) ? 2 : (ray_x[phase][i] < 0 ? -2 : 0));
        int16_t y2 = y1 + ((ray_y[phase][i] > 0) ? 2 : (ray_y[phase][i] < 0 ? -2 : 0));
        u8g2_DrawLine(u8g2, x1, y1, x2, y2);
    }

    if (hot != 0U)
    {
        int16_t wave_y = y + 24 + (int16_t)((phase & 0x01U) ? 1 : 0);
        u8g2_DrawHLine(u8g2, x + 8, wave_y, 7);
        u8g2_DrawHLine(u8g2, x + 18, wave_y + 2, 8);
    }
}

static void UI_Widget_DrawWeatherMoon(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t phase)
{
    int16_t cx = x + 18;
    int16_t cy = y + 11;

    u8g2_DrawDisc(u8g2, cx, cy, 6, U8G2_DRAW_ALL);
    u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawDisc(u8g2, cx + 3, cy - 1, 5, U8G2_DRAW_ALL);
    u8g2_SetDrawColor(u8g2, 1);

    if ((phase & 0x01U) == 0U)
    {
        u8g2_DrawPixel(u8g2, x + 7, y + 6);
        u8g2_DrawPixel(u8g2, x + 25, y + 4);
    }
    else
    {
        u8g2_DrawPixel(u8g2, x + 9, y + 4);
        u8g2_DrawPixel(u8g2, x + 24, y + 7);
    }
}

static void UI_Widget_DrawMoonPhase(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t phase)
{
    int16_t cx = x + 16;
    int16_t cy = y + 15;
    int16_t radius = 7;

    u8g2_DrawDisc(u8g2, cx, cy, radius, U8G2_DRAW_ALL);

    switch (phase)
    {
    case 0U:
        u8g2_SetDrawColor(u8g2, 0);
        u8g2_DrawDisc(u8g2, cx, cy, radius, U8G2_DRAW_ALL);
        break;

    case 1U:
        u8g2_SetDrawColor(u8g2, 0);
        u8g2_DrawDisc(u8g2, cx - 4, cy, 6, U8G2_DRAW_ALL);
        break;

    case 2U:
        u8g2_SetDrawColor(u8g2, 0);
        u8g2_DrawBox(u8g2, cx - radius, cy - radius, (uint8_t)radius, (uint8_t)(radius * 2 + 1));
        break;

    case 3U:
        u8g2_SetDrawColor(u8g2, 0);
        u8g2_DrawDisc(u8g2, cx - 8, cy, 5, U8G2_DRAW_ALL);
        break;

    case 4U:
        break;

    case 5U:
        u8g2_SetDrawColor(u8g2, 0);
        u8g2_DrawDisc(u8g2, cx + 8, cy, 5, U8G2_DRAW_ALL);
        break;

    case 6U:
        u8g2_SetDrawColor(u8g2, 0);
        u8g2_DrawBox(u8g2, cx + 1, cy - radius, (uint8_t)radius, (uint8_t)(radius * 2 + 1));
        break;

    case 7U:
    default:
        u8g2_SetDrawColor(u8g2, 0);
        u8g2_DrawDisc(u8g2, cx + 4, cy, 6, U8G2_DRAW_ALL);
        break;
    }

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawCircle(u8g2, cx, cy, radius, U8G2_DRAW_ALL);

    if ((phase & 0x01U) == 0U)
    {
        u8g2_DrawPixel(u8g2, x + 8, y + 6);
        u8g2_DrawPixel(u8g2, x + 24, y + 4);
    }
    else
    {
        u8g2_DrawPixel(u8g2, x + 10, y + 5);
        u8g2_DrawPixel(u8g2, x + 25, y + 7);
    }
}

static void UI_Widget_DrawWeatherCloudScene(u8g2_t *u8g2, int16_t x, int16_t y, const UI_WeatherStyle_t *style, uint8_t phase)
{
    if (style->night != 0U)
    {
        UI_Widget_DrawWeatherMoon(u8g2, x - 2, y, phase);
    }
    else if (style->kind == UI_WEATHER_ICON_PARTLY)
    {
        UI_Widget_DrawWeatherSun(u8g2, x - 1, y, phase, 0U);
    }

    UI_Widget_DrawWeatherCloudBase(u8g2, x, y + ((style->kind == UI_WEATHER_ICON_CLOUDY) ? 0 : 1), phase,
                                   (uint8_t)(style->level + ((style->kind == UI_WEATHER_ICON_CLOUDY) ? 1U : 0U)));
}

static void UI_Widget_DrawWeatherRain(u8g2_t *u8g2,
                                      int16_t x,
                                      int16_t y,
                                      const UI_WeatherStyle_t *style,
                                      uint8_t phase,
                                      UI_WeatherMotion_t motion)
{
    uint8_t i;
    uint8_t count = (uint8_t)(2U + style->level);
    uint8_t len = (uint8_t)(3U + (style->level >= 2U ? 1U : 0U) + (style->level >= 4U ? 1U : 0U));
    static const int8_t drop_x_pos[6] = { 8, 13, 18, 23, 27, 16 };

    count = UI_Widget_WeatherReduceForPair(count, motion, 2U);
    if (count > 6U)
    {
        count = 6U;
    }

    UI_Widget_DrawWeatherCloudBase(u8g2, x, y, phase, (uint8_t)(1U + (style->level >= 2U ? 1U : 0U)));

    for (i = 0; i < count; i++)
    {
        int16_t drop_x = x + drop_x_pos[i];
        int16_t drop_y = y + 21 + (int16_t)((phase + (i * 2U)) & 0x03U);

        if (style->mixed != 0U && (i & 0x01U) != 0U)
        {
            u8g2_DrawLine(u8g2, drop_x - 1, drop_y + 1, drop_x + 1, drop_y + 1);
            u8g2_DrawLine(u8g2, drop_x, drop_y, drop_x, drop_y + 2);
            continue;
        }

        u8g2_DrawLine(u8g2, drop_x, drop_y, drop_x, drop_y + len);
        if (style->level >= 3U)
        {
            u8g2_DrawPixel(u8g2, drop_x + 1, drop_y + 1);
        }
    }
}

static void UI_Widget_DrawWeatherSnow(u8g2_t *u8g2,
                                      int16_t x,
                                      int16_t y,
                                      const UI_WeatherStyle_t *style,
                                      uint8_t phase,
                                      UI_WeatherMotion_t motion)
{
    uint8_t i;
    uint8_t count = (uint8_t)(2U + style->level);
    static const int8_t flake_x_pos[6] = { 9, 14, 19, 24, 27, 16 };

    count = UI_Widget_WeatherReduceForPair(count, motion, 2U);
    if (count > 6U)
    {
        count = 6U;
    }

    UI_Widget_DrawWeatherCloudBase(u8g2, x, y, phase, (uint8_t)(1U + (style->level >= 2U ? 1U : 0U)));

    for (i = 0; i < count; i++)
    {
        int16_t fx = x + flake_x_pos[i];
        int16_t fy = y + 22 + (int16_t)(((phase + i) & 0x01U));

        if (style->mixed != 0U && (i & 0x01U) == 0U)
        {
            u8g2_DrawLine(u8g2, fx, fy, fx, fy + 3);
            continue;
        }

        u8g2_DrawLine(u8g2, fx - 1, fy, fx + 1, fy);
        u8g2_DrawLine(u8g2, fx, fy - 1, fx, fy + 1);
        if (style->level >= 2U)
        {
            u8g2_DrawPixel(u8g2, fx - 1, fy - 1);
            u8g2_DrawPixel(u8g2, fx + 1, fy + 1);
        }
    }
}

static void UI_Widget_DrawWeatherThunder(u8g2_t *u8g2,
                                         int16_t x,
                                         int16_t y,
                                         const UI_WeatherStyle_t *style,
                                         uint8_t phase,
                                         UI_WeatherMotion_t motion)
{
    uint8_t flash_phase = (uint8_t)(phase & 0x03U);

    UI_Widget_DrawWeatherRain(u8g2, x, y, style, phase, motion);

    if ((style->severe == 0U && flash_phase >= 2U) ||
        (style->severe != 0U && flash_phase == 3U))
    {
        return;
    }

    u8g2_DrawLine(u8g2, x + 18, y + 17, x + 14, y + 24);
    u8g2_DrawLine(u8g2, x + 14, y + 24, x + 19, y + 24);
    u8g2_DrawLine(u8g2, x + 19, y + 24, x + 16, y + 30);
    u8g2_DrawLine(u8g2, x + 16, y + 30, x + 22, y + 21);
}

static void UI_Widget_DrawWeatherFog(u8g2_t *u8g2,
                                     int16_t x,
                                     int16_t y,
                                     const UI_WeatherStyle_t *style,
                                     uint8_t phase,
                                     UI_WeatherMotion_t motion)
{
    uint8_t lines = (uint8_t)(2U + (style->level >= 2U ? 1U : 0U) + (style->level >= 4U ? 1U : 0U));
    uint8_t i;

    lines = UI_Widget_WeatherReduceForPair(lines, motion, 2U);

    UI_Widget_DrawWeatherCloudBase(u8g2, x, y + 1, phase, (uint8_t)(1U + (style->level >= 3U ? 1U : 0U)));

    for (i = 0; i < lines; i++)
    {
        int16_t line_y = y + 22 + (int16_t)(i * 3);
        int16_t shift = (int16_t)(((phase + i) & 0x01U) ? 1 : 0);
        int16_t start_x = x + 7 + ((i & 0x01U) ? shift : -shift);
        uint8_t len = (uint8_t)(18U + ((style->level >= 3U && i == 0U) ? 2U : 0U));

        u8g2_DrawHLine(u8g2, start_x, line_y, len);
    }
}

static void UI_Widget_DrawWeatherDust(u8g2_t *u8g2,
                                      int16_t x,
                                      int16_t y,
                                      const UI_WeatherStyle_t *style,
                                      uint8_t phase,
                                      UI_WeatherMotion_t motion)
{
    uint8_t bands = (uint8_t)(2U + (style->level >= 2U ? 1U : 0U));
    uint8_t dots = (uint8_t)(2U + (style->level >= 3U ? 2U : 0U));
    uint8_t i;

    bands = UI_Widget_WeatherReduceForPair(bands, motion, 2U);
    dots = UI_Widget_WeatherReduceForPair(dots, motion, 2U);

    u8g2_DrawHLine(u8g2, x + 6, y + 11, 20);
    u8g2_DrawHLine(u8g2, x + 10, y + 15, 14);

    for (i = 0; i < bands; i++)
    {
        int16_t line_y = y + 20 + (int16_t)(i * 3);
        int16_t shift = (int16_t)(((phase + i) & 0x01U) ? 1 : 0);
        u8g2_DrawHLine(u8g2, x + 7 + shift, line_y, (uint8_t)(16U + i));
    }

    for (i = 0; i < dots; i++)
    {
        int16_t px = x + 9 + (int16_t)((i * 5U + phase) % 17U);
        int16_t py = y + 13 + (int16_t)((i & 0x01U) ? 4 : 1);
        u8g2_DrawPixel(u8g2, px, py);
    }
}

static void UI_Widget_DrawWeatherWind(u8g2_t *u8g2,
                                      int16_t x,
                                      int16_t y,
                                      const UI_WeatherStyle_t *style,
                                      uint8_t phase,
                                      UI_WeatherMotion_t motion)
{
    uint8_t lines = (uint8_t)(2U + (style->level >= 2U ? 1U : 0U) + (style->level >= 3U ? 1U : 0U));
    uint8_t i;

    lines = UI_Widget_WeatherReduceForPair(lines, motion, 2U);

    for (i = 0; i < lines; i++)
    {
        int16_t yy = y + 10 + (int16_t)(i * 5);
        int16_t lead = x + 5 + (int16_t)(((phase + i) & 0x03U));
        uint8_t len = (uint8_t)(12U + i * 2U);

        u8g2_DrawHLine(u8g2, lead, yy, len);
        u8g2_DrawLine(u8g2, lead + len, yy, lead + len + 2, yy - 1);
        if (style->level >= 3U)
        {
            u8g2_DrawLine(u8g2, lead + 2, yy + 2, lead + 8, yy + 2);
        }
    }
}

static void UI_Widget_DrawWeatherCold(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t phase)
{
    int16_t cx = x + 16;
    int16_t cy = y + 15;

    u8g2_DrawLine(u8g2, cx - 5, cy, cx + 5, cy);
    u8g2_DrawLine(u8g2, cx, cy - 5, cx, cy + 5);
    u8g2_DrawLine(u8g2, cx - 4, cy - 4, cx + 4, cy + 4);
    u8g2_DrawLine(u8g2, cx - 4, cy + 4, cx + 4, cy - 4);
    u8g2_DrawHLine(u8g2, x + 7 + (int16_t)(phase & 0x01U), y + 25, 10);
    u8g2_DrawHLine(u8g2, x + 18 - (int16_t)(phase & 0x01U), y + 27, 8);
}

static uint8_t UI_Widget_DrawWeatherDynamic(u8g2_t *u8g2,
                                            int16_t x,
                                            int16_t y,
                                            int icon_id,
                                            uint8_t phase_offset,
                                            UI_WeatherMotion_t motion)
{
    UI_WeatherStyle_t style = UI_Widget_WeatherResolve(icon_id);
    uint8_t phase = UI_Widget_WeatherPhaseWithOffset(UI_Widget_WeatherAnimPhaseBase(), phase_offset);

    switch (style.kind)
    {
    case UI_WEATHER_ICON_CLEAR:
        if (style.night != 0U)
        {
            UI_Widget_DrawWeatherMoon(u8g2, x, y, phase);
        }
        else
        {
            UI_Widget_DrawWeatherSun(u8g2, x, y, phase, 0U);
        }
        return 1U;

    case UI_WEATHER_ICON_MOON_PHASE:
        UI_Widget_DrawMoonPhase(u8g2, x, y, style.level);
        return 1U;

    case UI_WEATHER_ICON_PARTLY:
    case UI_WEATHER_ICON_CLOUDY:
        UI_Widget_DrawWeatherCloudScene(u8g2, x, y, &style, phase);
        return 1U;

    case UI_WEATHER_ICON_RAIN:
        UI_Widget_DrawWeatherRain(u8g2, x, y, &style, phase, motion);
        return 1U;

    case UI_WEATHER_ICON_SNOW:
        UI_Widget_DrawWeatherSnow(u8g2, x, y, &style, phase, motion);
        return 1U;

    case UI_WEATHER_ICON_THUNDER:
        UI_Widget_DrawWeatherThunder(u8g2, x, y, &style, phase, motion);
        return 1U;

    case UI_WEATHER_ICON_FOG:
        UI_Widget_DrawWeatherFog(u8g2, x, y, &style, phase, motion);
        return 1U;

    case UI_WEATHER_ICON_DUST:
        UI_Widget_DrawWeatherDust(u8g2, x, y, &style, phase, motion);
        return 1U;

    case UI_WEATHER_ICON_WIND:
        UI_Widget_DrawWeatherWind(u8g2, x, y, &style, phase, motion);
        return 1U;

    case UI_WEATHER_ICON_HOT:
        UI_Widget_DrawWeatherSun(u8g2, x, y, phase, 1U);
        return 1U;

    case UI_WEATHER_ICON_COLD:
        UI_Widget_DrawWeatherCold(u8g2, x, y, phase);
        return 1U;

    default:
        return 0U;
    }
}

static void UI_Widget_DrawWeatherIconOrFallback(u8g2_t *u8g2,
                                                int16_t x,
                                                int16_t y,
                                                int icon_id,
                                                uint8_t phase_offset,
                                                UI_WeatherMotion_t motion)
{
    const uint8_t *icon = NULL;

    if (UI_Widget_DrawWeatherDynamic(u8g2, x, y, icon_id, phase_offset, motion) != 0U)
    {
        return;
    }

    icon = UI_Widget_IconPtr(icon_id);
    if (icon == NULL)
    {
        icon = UI_Widget_IconPtr(999);
    }

    if (icon != NULL)
    {
        u8g2_DrawXBM(u8g2, x, y, 32, 32, icon);
    }
}
/* 天气与首页默认区域 widget。 */
void UI_Widget_DrawHomeDefaultLeft(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    (void)w;
    (void)h;

    UI_Widget_DrawWeatherIconOrFallback(u8g2, x, y, g_now_weather.icon, 0U, UI_WEATHER_MOTION_MAIN);
}

void UI_Widget_DrawHomeDefaultMiddle(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    (void)w;
    (void)h;
    u8g2_SetFont(u8g2, u8g2_font_5x8_tf);
    UI_Widget_DrawCenteredStr(u8g2, x, y + 12, 64, "TIME");
    UI_Widget_DrawCenteredStr(u8g2, x, y + 27, 64, "HOME");
}

void UI_Widget_DrawRegionWeather(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    (void)w;
    (void)h;
    u8g2_SetFont(u8g2, u8g2_font_5x8_tf);
    UI_Widget_DrawCenteredStr(u8g2, x, y + 12, 32, "WTH");
    UI_Widget_DrawCenteredWeatherStr(u8g2, x, y + 27, 32, g_now_weather.text);
}

void UI_Widget_DrawRegionTemp(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    char buf[8];
    (void)h;

    u8g2_SetFont(u8g2, u8g2_font_5x8_tf);
    snprintf(buf, sizeof(buf), "%d°C", g_now_weather.temp);
    UI_Widget_DrawCenteredStr(u8g2, x, y + 10, w, "TEMP");
    UI_Widget_DrawCenteredWeatherStr(u8g2, x, y + 27, w, buf);
}
// 此处要读取板子上的温度，还需要完善
/* TODO: replace demo value with on-board temperature source. */
void UI_Widget_DrawRegionTempNow(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    char buf[8];
    (void)h;

    u8g2_SetFont(u8g2, u8g2_font_5x8_tf);
    snprintf(buf, sizeof(buf), "%dC", UI_Widget_RoundFloatToInt(g_sensors_environment.temp));
    UI_Widget_DrawCenteredStr(u8g2, x, y + 10, w, "NOW");
    UI_Widget_DrawCenteredWeatherStr(u8g2, x, y + 27, w, buf);
}


/**
 * @brief  今明最高温（例：HI: 23°C）
 */
void UI_Widget_DrawRegionHighTemp(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    char buf[12];
    (void)h;
    u8g2_SetFont(u8g2, u8g2_font_5x8_tf);
    UI_Widget_DrawCenteredStr(u8g2, x, y + 10, w, "HI");
    snprintf(buf, sizeof(buf), "%d°C", g_future_weather[0].temp_high);
    UI_Widget_DrawCenteredWeatherStr(u8g2, x, y + 27, 32, buf);
}

/**
 * @brief  今明最低温（例：LO: 20°C）
 */
void UI_Widget_DrawRegionLowTemp(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    char buf[12];
    (void)h;
    u8g2_SetFont(u8g2, u8g2_font_5x8_tf);
    UI_Widget_DrawCenteredStr(u8g2, x, y + 10, w, "LO");
    snprintf(buf, sizeof(buf), "%d°C", g_future_weather[0].temp_low);
    UI_Widget_DrawCenteredWeatherStr(u8g2, x, y + 27, 32, buf);
}

/**
 * @brief  PM2.5（例：PM2.5 / 13）
 */
void UI_Widget_DrawRegionPM25(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    char buf[12];
    (void)h;
    u8g2_SetFont(u8g2, u8g2_font_5x8_tf);
    UI_Widget_DrawCenteredStr(u8g2, x, y + 10, w, "PM2.5");
    snprintf(buf, sizeof(buf), "%d", g_now_weather.pm25);
    UI_Widget_DrawCenteredWeatherStr(u8g2, x, y + 27, 32, buf);
}

/**
 * @brief  湿度（例：HUM: 15%）
 */
void UI_Widget_DrawRegionHumidity(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    char buf[12];
    (void)h;
    u8g2_SetFont(u8g2, u8g2_font_5x8_tf);
    UI_Widget_DrawCenteredStr(u8g2, x, y + 10, w, "HUM");
    snprintf(buf, sizeof(buf), "%d%%", g_now_weather.humidity);
    UI_Widget_DrawCenteredWeatherStr(u8g2, x, y + 27, 32, buf);
}

/**
 * @brief  湿度（例：HUM: 15%）
 */
// 此处要读取板子上的湿度，要完善
/* TODO: replace demo value with on-board humidity source. */
void UI_Widget_DrawRegionHumidityNow(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    char buf[12];
    (void)h;
    u8g2_SetFont(u8g2, u8g2_font_5x8_tf);
    UI_Widget_DrawCenteredStr(u8g2, x, y + 10, w, "NOW");
    snprintf(buf, sizeof(buf), "%d%%", UI_Widget_LocalHumidityPercent());
    UI_Widget_DrawCenteredWeatherStr(u8g2, x, y + 27, 32, buf);
}

/**
 * @brief  天气图标 32×32
 */
void UI_Widget_DrawRegionIcon(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    (void)w;
    (void)h;

    UI_Widget_DrawWeatherIconOrFallback(u8g2, x, y, g_now_weather.icon, 0U, UI_WEATHER_MOTION_MAIN);
}
/**
 * @brief  大后天天气图标 32×32
 */
void UI_Widget_DrawRegionIcon3(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    (void)w;
    (void)h;

    UI_Widget_DrawWeatherIconOrFallback(u8g2, x, y, g_future_weather[3].icon_id, 2U, UI_WEATHER_MOTION_FUTURE_SINGLE);
}
/**
 * @brief  明天后天天气图标 32×32
 */
void UI_Widget_DrawRegionIcon1_2(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    (void)w;
    (void)h;

    UI_Widget_DrawWeatherIconOrFallback(u8g2, x, y, g_future_weather[1].icon_id, 1U, UI_WEATHER_MOTION_FUTURE_PAIR);
    UI_Widget_DrawWeatherIconOrFallback(u8g2, x + 32, y, g_future_weather[2].icon_id, 5U, UI_WEATHER_MOTION_FUTURE_PAIR);
}

/**
 * @brief  中栏：温度 + 湿度（例：TMP:22°C HUM:15%）
 */
void UI_Widget_DrawRegionTempHumid(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    char buf[24];
    (void)h;
    u8g2_SetFont(u8g2, u8g2_font_6x10_tf);
    snprintf(buf, sizeof(buf), "%d°C  %d%%", g_now_weather.temp, g_now_weather.humidity);
    UI_Widget_DrawCenteredStr(u8g2, x, y + 12, w, "TEMP   HUM");
    UI_Widget_DrawCenteredStr(u8g2, x, y + 27, w, buf);
}

void UI_Widget_DrawRegionTempHumidSmall(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    char buf[12];
    (void)h;

    u8g2_SetFont(u8g2, u8g2_font_5x8_tf);

    // ========== 温度显示 ==========
    memset(buf, 0, sizeof(buf)); // 清空缓冲区
    UI_Widget_FormatTempTenths(buf, sizeof(buf), g_sensors_environment.temp);
    UI_Widget_DrawCenteredStr(u8g2, x, y + 14, w, buf);

    // ========== 湿度显示 ==========
    memset(buf, 0, sizeof(buf)); // 再次清空，避免温度字符串残留干扰湿度
    snprintf(buf, sizeof(buf), "%d%%", UI_Widget_LocalHumidityPercent());
    UI_Widget_DrawCenteredStr(u8g2, x, y + 28, w, buf);
}

/**
 * @brief  中栏：天气描述 + PM2.5（例：大雨  PM2.5:13）
 */
void UI_Widget_DrawRegionWeatherPM(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    char buf[24];
    (void)h;
    snprintf(buf, sizeof(buf), "PM2.5:%d", g_now_weather.pm25);
    UI_Widget_DrawCenteredWeatherStr(u8g2, x, y + 12, w, g_now_weather.text);
    u8g2_SetFont(u8g2, u8g2_font_6x10_tf);
    UI_Widget_DrawCenteredStr(u8g2, x, y + 27, w, buf);
}
