#include "../ui_widget.h"

#if 0

#define UI_ROLL_GLYPH_W              12U
#define UI_ROLL_GLYPH_H              24U
#define UI_ROLL_GLYPH_PAGE_COUNT     3U
#define UI_ROLL_GLYPH_BYTES          (UI_ROLL_GLYPH_W * UI_ROLL_GLYPH_PAGE_COUNT)

enum {
    UI_ROLL_SEG_TOP = 0x01,
    UI_ROLL_SEG_UPPER_LEFT = 0x02,
    UI_ROLL_SEG_UPPER_RIGHT = 0x04,
    UI_ROLL_SEG_MIDDLE = 0x08,
    UI_ROLL_SEG_LOWER_LEFT = 0x10,
    UI_ROLL_SEG_LOWER_RIGHT = 0x20,
    UI_ROLL_SEG_BOTTOM = 0x40
};

static uint8_t g_ui_roll_font_ready;
static uint8_t g_ui_roll_digit_glyphs[10][UI_ROLL_GLYPH_BYTES];
static uint8_t g_ui_roll_colon_glyph[UI_ROLL_GLYPH_BYTES];

static void UI_RollFontFillRect(uint8_t *glyph, uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    uint8_t px;
    uint8_t py;

    if (glyph == NULL)
    {
        return;
    }

    for (py = 0; py < h; py++)
    {
        uint8_t draw_y = (uint8_t)(y + py);

        if (draw_y >= UI_ROLL_GLYPH_H)
        {
            break;
        }

        for (px = 0; px < w; px++)
        {
            uint8_t draw_x = (uint8_t)(x + px);

            if (draw_x >= UI_ROLL_GLYPH_W)
            {
                break;
            }

            glyph[((draw_y >> 3) * UI_ROLL_GLYPH_W) + draw_x] |= (uint8_t)(1U << (draw_y & 0x07U));
        }
    }
}

static void UI_RollFontBuildDigitGlyph(uint8_t value, uint8_t *glyph)
{
    static const uint8_t digit_segments[10] = {
        UI_ROLL_SEG_TOP | UI_ROLL_SEG_UPPER_LEFT | UI_ROLL_SEG_UPPER_RIGHT | UI_ROLL_SEG_LOWER_LEFT | UI_ROLL_SEG_LOWER_RIGHT | UI_ROLL_SEG_BOTTOM,
        UI_ROLL_SEG_UPPER_RIGHT | UI_ROLL_SEG_LOWER_RIGHT,
        UI_ROLL_SEG_TOP | UI_ROLL_SEG_UPPER_RIGHT | UI_ROLL_SEG_MIDDLE | UI_ROLL_SEG_LOWER_LEFT | UI_ROLL_SEG_BOTTOM,
        UI_ROLL_SEG_TOP | UI_ROLL_SEG_UPPER_RIGHT | UI_ROLL_SEG_MIDDLE | UI_ROLL_SEG_LOWER_RIGHT | UI_ROLL_SEG_BOTTOM,
        UI_ROLL_SEG_UPPER_LEFT | UI_ROLL_SEG_UPPER_RIGHT | UI_ROLL_SEG_MIDDLE | UI_ROLL_SEG_LOWER_RIGHT,
        UI_ROLL_SEG_TOP | UI_ROLL_SEG_UPPER_LEFT | UI_ROLL_SEG_MIDDLE | UI_ROLL_SEG_LOWER_RIGHT | UI_ROLL_SEG_BOTTOM,
        UI_ROLL_SEG_TOP | UI_ROLL_SEG_UPPER_LEFT | UI_ROLL_SEG_MIDDLE | UI_ROLL_SEG_LOWER_LEFT | UI_ROLL_SEG_LOWER_RIGHT | UI_ROLL_SEG_BOTTOM,
        UI_ROLL_SEG_TOP | UI_ROLL_SEG_UPPER_RIGHT | UI_ROLL_SEG_LOWER_RIGHT,
        UI_ROLL_SEG_TOP | UI_ROLL_SEG_UPPER_LEFT | UI_ROLL_SEG_UPPER_RIGHT | UI_ROLL_SEG_MIDDLE | UI_ROLL_SEG_LOWER_LEFT | UI_ROLL_SEG_LOWER_RIGHT | UI_ROLL_SEG_BOTTOM,
        UI_ROLL_SEG_TOP | UI_ROLL_SEG_UPPER_LEFT | UI_ROLL_SEG_UPPER_RIGHT | UI_ROLL_SEG_MIDDLE | UI_ROLL_SEG_LOWER_RIGHT | UI_ROLL_SEG_BOTTOM
    };
    uint8_t seg;

    if ((glyph == NULL) || (value >= 10U))
    {
        return;
    }

    memset(glyph, 0, UI_ROLL_GLYPH_BYTES);
    seg = digit_segments[value];

    if ((seg & UI_ROLL_SEG_TOP) != 0U)
    {
        UI_RollFontFillRect(glyph, 2U, 0U, 8U, 3U);
    }
    if ((seg & UI_ROLL_SEG_UPPER_LEFT) != 0U)
    {
        UI_RollFontFillRect(glyph, 0U, 3U, 3U, 8U);
    }
    if ((seg & UI_ROLL_SEG_UPPER_RIGHT) != 0U)
    {
        UI_RollFontFillRect(glyph, 9U, 3U, 3U, 8U);
    }
    if ((seg & UI_ROLL_SEG_MIDDLE) != 0U)
    {
        UI_RollFontFillRect(glyph, 2U, 10U, 8U, 3U);
    }
    if ((seg & UI_ROLL_SEG_LOWER_LEFT) != 0U)
    {
        UI_RollFontFillRect(glyph, 0U, 13U, 3U, 8U);
    }
    if ((seg & UI_ROLL_SEG_LOWER_RIGHT) != 0U)
    {
        UI_RollFontFillRect(glyph, 9U, 13U, 3U, 8U);
    }
    if ((seg & UI_ROLL_SEG_BOTTOM) != 0U)
    {
        UI_RollFontFillRect(glyph, 2U, 21U, 8U, 3U);
    }
}

static void UI_RollFontEnsureReady(void)
{
    uint8_t i;

    if (g_ui_roll_font_ready)
    {
        return;
    }

    for (i = 0; i < 10U; i++)
    {
        UI_RollFontBuildDigitGlyph(i, g_ui_roll_digit_glyphs[i]);
    }

    memset(g_ui_roll_colon_glyph, 0, sizeof(g_ui_roll_colon_glyph));
    UI_RollFontFillRect(g_ui_roll_colon_glyph, 4U, 6U, 3U, 3U);
    UI_RollFontFillRect(g_ui_roll_colon_glyph, 4U, 15U, 3U, 3U);
    g_ui_roll_font_ready = 1U;
}

static const uint8_t *UI_RollFontDigit(uint8_t value)
{
    UI_RollFontEnsureReady();

    if (value >= 10U)
    {
        return g_ui_roll_digit_glyphs[0];
    }

    return g_ui_roll_digit_glyphs[value];
}

static const uint8_t *UI_RollFontColon(void)
{
    UI_RollFontEnsureReady();
    return g_ui_roll_colon_glyph;
}

#endif

/*
 * 首页滚动时钟 widget。
 * 通过 mid_digits/sec_digits 两组数字，实现“时:分”和“:秒”的独立滚动动画。
 */

static uint8_t UI_Widget_IsClockRollVisible(void)
{
    if (g_ui.cur_page == PAGE_CLOCK)
    {
        return 1U;
    }

    return ((g_ui.cur_page == PAGE_HOME) &&
            ((g_ui_regions[1].current_draw == UI_Widget_DrawHomeClockMid) ||
             (g_ui_regions[2].current_draw == UI_Widget_DrawHomeClockSec) ||
             (g_ui_regions[1].next_draw == UI_Widget_DrawHomeClockMid) ||
             (g_ui_regions[2].next_draw == UI_Widget_DrawHomeClockSec))) ? 1U : 0U;
}

/* 根据当前时间重建滚动时钟的基准状态。 */
static uint8_t UI_Widget_InitClockRoll(const time_t *now_time, TickType_t now)
{
    (void)now;

    if (now_time == NULL)
    {
        return 0U;
    }

    g_ui_clock_roll.ready = 1U;
    g_ui_clock_roll.last_hour = now_time->hour;
    g_ui_clock_roll.last_min = now_time->min;
    g_ui_clock_roll.last_sec = now_time->sec;

    g_ui_clock_roll.mid_digits[0].cur = (uint8_t)(now_time->hour / 10U);
    g_ui_clock_roll.mid_digits[1].cur = (uint8_t)(now_time->hour % 10U);
    g_ui_clock_roll.mid_digits[2].cur = (uint8_t)(now_time->min / 10U);
    g_ui_clock_roll.mid_digits[3].cur = (uint8_t)(now_time->min % 10U);
    g_ui_clock_roll.sec_digits[0].cur = (uint8_t)(now_time->sec / 10U);
    g_ui_clock_roll.sec_digits[1].cur = (uint8_t)(now_time->sec % 10U);

    g_ui_clock_roll.mid_digits[0].next = g_ui_clock_roll.mid_digits[0].cur;
    g_ui_clock_roll.mid_digits[1].next = g_ui_clock_roll.mid_digits[1].cur;
    g_ui_clock_roll.mid_digits[2].next = g_ui_clock_roll.mid_digits[2].cur;
    g_ui_clock_roll.mid_digits[3].next = g_ui_clock_roll.mid_digits[3].cur;
    g_ui_clock_roll.sec_digits[0].next = g_ui_clock_roll.sec_digits[0].cur;
    g_ui_clock_roll.sec_digits[1].next = g_ui_clock_roll.sec_digits[1].cur;

    return 1U;
}

/* 把单个数字初始化为静止状态。 */
static void UI_Widget_ClockDigitInit(UI_ClockDigit_t *digit, uint8_t value)
{
    if (digit == NULL)
    {
        return;
    }

    digit->cur = value;
    digit->next = value;
    digit->anim = 0U;
    digit->offset = 0;
    digit->dir = UI_SLIDE_UP;
    digit->next_frame_tick = 0;
}

/* 当数值发生变化时，给单个数字启动一次滚动动画。 */
static void UI_Widget_ClockDigitStart(UI_ClockDigit_t *digit, uint8_t value, TickType_t now)
{
    if (digit == NULL)
    {
        return;
    }

    if ((digit->cur == value) && !digit->anim)
    {
        digit->next = value;
        return;
    }

    digit->next = value;
    digit->anim = 1U;
    digit->offset = 0;
    digit->dir = UI_SLIDE_DOWN;
    digit->next_frame_tick = now;
}

/* 按固定步长推进单个数字的滚动。 */
static void UI_Widget_ClockDigitUpdate(UI_ClockDigit_t *digit, TickType_t now)
{
    TickType_t frame_ticks;

    if ((digit == NULL) || !digit->anim)
    {
        return;
    }

    frame_ticks = pdMS_TO_TICKS(OLED_UI_CLOCK_ROLL_FRAME_MS);
    if (frame_ticks == 0U)
    {
        frame_ticks = 1U;
    }

    while ((int32_t)(now - digit->next_frame_tick) >= 0)
    {
        digit->offset += OLED_UI_CLOCK_ROLL_STEP_PX;
        digit->next_frame_tick += frame_ticks;

        if (digit->offset >= 24)
        {
            digit->cur = digit->next;
            digit->anim = 0U;
            digit->offset = 0;
            break;
        }
    }
}

/* 直接按像素绘制滚动时钟字模。 */
static void UI_Widget_DrawRollGlyph(u8g2_t *u8g2, int16_t x, int16_t y, const uint8_t *glyph)
{
    uint8_t page;
    uint8_t col;
    uint8_t bit;

    if (glyph == NULL)
    {
        return;
    }

    for (page = 0; page < 3U; page++)
    {
        for (col = 0; col < 12U; col++)
        {
            uint8_t data = glyph[(page * 12U) + col];

            if (data == 0U)
            {
                continue;
            }

            for (bit = 0; bit < 8U; bit++)
            {
                if ((data & (uint8_t)(1U << bit)) != 0U)
                {
                    u8g2_DrawPixel(u8g2, x + col, y + (page * 8U) + bit);
                }
            }
        }
    }
}

/* 绘制单个数字当前帧，既支持静止也支持上下滚动。 */
static void UI_Widget_DrawClockDigit(u8g2_t *u8g2, int16_t x, int16_t y, const UI_ClockDigit_t *digit)
{
    const uint8_t *cur_glyph;
    const uint8_t *next_glyph;
    int16_t cur_y;
    int16_t next_y;

    if (digit == NULL)
    {
        return;
    }

    cur_glyph = UI_RollFontDigit(digit->cur);
    next_glyph = UI_RollFontDigit(digit->next);

    if (!digit->anim)
    {
        UI_Widget_DrawRollGlyph(u8g2, x, y, cur_glyph);
        return;
    }

    if (digit->dir == UI_SLIDE_DOWN)
    {
        cur_y = y + digit->offset;
        next_y = y + digit->offset - 24;
    }
    else
    {
        cur_y = y - digit->offset;
        next_y = y + 24 - digit->offset;
    }

    UI_Widget_DrawRollGlyph(u8g2, x, cur_y, cur_glyph);
    UI_Widget_DrawRollGlyph(u8g2, x, next_y, next_glyph);
}

/**
 * @brief  停止并清空滚动时钟状态
 * @retval None
 */
void UI_Widget_StopClockRoll(void)
{
    memset(&g_ui_clock_roll, 0, sizeof(g_ui_clock_roll));
}

/**
 * @brief  把首页中间/右侧区域切换为滚动时钟
 * @param  now: 当前 tick
 * @retval 1 表示成功切换
 */
uint8_t UI_Widget_StartClockRollPair(TickType_t now)
{
    time_t now_time;

    if ((g_ui.cur_page != PAGE_HOME) || g_ui_page_trans.active)
    {
        return 0U;
    }

    Time_Get(&now_time);
    if (!g_ui_clock_roll.ready)
    {
        UI_Widget_ClockDigitInit(&g_ui_clock_roll.mid_digits[0], (uint8_t)(now_time.hour / 10U));
        UI_Widget_ClockDigitInit(&g_ui_clock_roll.mid_digits[1], (uint8_t)(now_time.hour % 10U));
        UI_Widget_ClockDigitInit(&g_ui_clock_roll.mid_digits[2], (uint8_t)(now_time.min / 10U));
        UI_Widget_ClockDigitInit(&g_ui_clock_roll.mid_digits[3], (uint8_t)(now_time.min % 10U));
        UI_Widget_ClockDigitInit(&g_ui_clock_roll.sec_digits[0], (uint8_t)(now_time.sec / 10U));
        UI_Widget_ClockDigitInit(&g_ui_clock_roll.sec_digits[1], (uint8_t)(now_time.sec % 10U));
        (void)UI_Widget_InitClockRoll(&now_time, now);
    }

    g_ui_regions[1].current_draw = UI_Widget_DrawHomeClockMid;
    g_ui_regions[1].next_draw = NULL;
    g_ui_regions[1].state = UI_REGION_IDLE;
    g_ui_regions[1].slide_offset = 0;
    g_ui_regions[1].next_frame_tick = now;

    g_ui_regions[2].current_draw = UI_Widget_DrawHomeClockSec;
    g_ui_regions[2].next_draw = NULL;
    g_ui_regions[2].state = UI_REGION_IDLE;
    g_ui_regions[2].slide_offset = 0;
    g_ui_regions[2].next_frame_tick = now;

    return 1U;
}

/**
 * @brief  每帧推进滚动时钟动画
 * @retval None
 */
void UI_Widget_UpdateClockRoll(void)
{
    time_t now_time;
    TickType_t now;
    uint8_t values[4];
    uint8_t sec_values[2];
    uint8_t i;

    if (!UI_Widget_IsClockRollVisible())
    {
        return;
    }

    Time_Get(&now_time);
    now = xTaskGetTickCount();

    if (!g_ui_clock_roll.ready)
    {
        UI_Widget_ClockDigitInit(&g_ui_clock_roll.mid_digits[0], (uint8_t)(now_time.hour / 10U));
        UI_Widget_ClockDigitInit(&g_ui_clock_roll.mid_digits[1], (uint8_t)(now_time.hour % 10U));
        UI_Widget_ClockDigitInit(&g_ui_clock_roll.mid_digits[2], (uint8_t)(now_time.min / 10U));
        UI_Widget_ClockDigitInit(&g_ui_clock_roll.mid_digits[3], (uint8_t)(now_time.min % 10U));
        UI_Widget_ClockDigitInit(&g_ui_clock_roll.sec_digits[0], (uint8_t)(now_time.sec / 10U));
        UI_Widget_ClockDigitInit(&g_ui_clock_roll.sec_digits[1], (uint8_t)(now_time.sec % 10U));
        (void)UI_Widget_InitClockRoll(&now_time, now);
        return;
    }

    values[0] = (uint8_t)(now_time.hour / 10U);
    values[1] = (uint8_t)(now_time.hour % 10U);
    values[2] = (uint8_t)(now_time.min / 10U);
    values[3] = (uint8_t)(now_time.min % 10U);
    sec_values[0] = (uint8_t)(now_time.sec / 10U);
    sec_values[1] = (uint8_t)(now_time.sec % 10U);

    for (i = 0; i < 4U; i++)
    {
        if ((!g_ui_clock_roll.mid_digits[i].anim && g_ui_clock_roll.mid_digits[i].cur != values[i]) ||
            (g_ui_clock_roll.mid_digits[i].anim && g_ui_clock_roll.mid_digits[i].next != values[i]))
        {
            UI_Widget_ClockDigitStart(&g_ui_clock_roll.mid_digits[i], values[i], now);
        }
    }

    for (i = 0; i < 2U; i++)
    {
        if ((!g_ui_clock_roll.sec_digits[i].anim && g_ui_clock_roll.sec_digits[i].cur != sec_values[i]) ||
            (g_ui_clock_roll.sec_digits[i].anim && g_ui_clock_roll.sec_digits[i].next != sec_values[i]))
        {
            UI_Widget_ClockDigitStart(&g_ui_clock_roll.sec_digits[i], sec_values[i], now);
        }
    }

    for (i = 0; i < 4U; i++)
    {
        UI_Widget_ClockDigitUpdate(&g_ui_clock_roll.mid_digits[i], now);
    }

    for (i = 0; i < 2U; i++)
    {
        UI_Widget_ClockDigitUpdate(&g_ui_clock_roll.sec_digits[i], now);
    }

    g_ui_clock_roll.last_hour = now_time.hour;
    g_ui_clock_roll.last_min = now_time.min;
    g_ui_clock_roll.last_sec = now_time.sec;
}

/**
 * @brief  绘制“时:分”区域
 */
void UI_Widget_DrawHomeClockMid(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    (void)w;
    (void)h;

    UI_Widget_DrawClockDigit(u8g2, x + 0, y + 4, &g_ui_clock_roll.mid_digits[0]);
    UI_Widget_DrawClockDigit(u8g2, x + 15, y + 4, &g_ui_clock_roll.mid_digits[1]);
    UI_Widget_DrawRollGlyph(u8g2, x + 28, y + 4, UI_RollFontColon());
    UI_Widget_DrawClockDigit(u8g2, x + 37, y + 4, &g_ui_clock_roll.mid_digits[2]);
    UI_Widget_DrawClockDigit(u8g2, x + 52, y + 4, &g_ui_clock_roll.mid_digits[3]);
}

/**
 * @brief  绘制“:秒”区域
 */
void UI_Widget_DrawHomeClockSec(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    (void)w;
    (void)h;

    UI_Widget_DrawRollGlyph(u8g2, x + 0, y + 4, UI_RollFontColon());
    UI_Widget_DrawClockDigit(u8g2, x + 8, y + 4, &g_ui_clock_roll.sec_digits[0]);
    UI_Widget_DrawClockDigit(u8g2, x + 20, y + 4, &g_ui_clock_roll.sec_digits[1]);
}
