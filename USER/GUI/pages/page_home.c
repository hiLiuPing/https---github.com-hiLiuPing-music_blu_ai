#include "../ui_page.h"
#include "../ui_widget.h"

/*
 * Boot, home, and clock page drawing stay together here to avoid adding
 * another Keil compilation unit for a small feature.
 */

static uint8_t UI_Boot_GetProgress(void)
{
    uint32_t total_frames = OLED_UI_BOOT_TIME_MS / OLED_UI_BOOT_FRAME_MS;
    uint32_t progress;

    if (total_frames == 0U)
    {
        return 100U;
    }

    progress = ((uint32_t)g_ui_boot_frame * 100U) / total_frames;
    return (progress > 100U) ? 100U : (uint8_t)progress;
}

static void UI_Boot_DrawProgressRail(u8g2_t *u8g2, int16_t x_off, uint8_t progress)
{
    uint8_t w = (uint8_t)((uint32_t)100U * progress / 100U);

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawHLine(u8g2, x_off + 14, 28, 100);
    if (w > 0U)
    {
        u8g2_DrawBox(u8g2, x_off + 14, 27, w, 3);
    }
}

static void UI_Boot_DrawScanline(u8g2_t *u8g2, int16_t x_off, uint8_t progress)
{
    uint8_t i;
    int16_t scan_x = x_off + 8 + (int16_t)((uint32_t)112U * progress / 100U);
    uint8_t pulse = (uint8_t)(g_ui_boot_frame % 8U);

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawFrame(u8g2, x_off + 2, 2, 124, 28);
    for (i = 0; i < 5U; i++)
    {
        int16_t y = (int16_t)(6 + i * 4);
        u8g2_DrawHLine(u8g2, x_off + 10 + i * 4, y, 76 - i * 6);
    }

    u8g2_DrawVLine(u8g2, scan_x, 4, 23);
    if (scan_x > x_off + 3)
    {
        u8g2_DrawVLine(u8g2, scan_x - 2, 6, 19);
    }

    u8g2_DrawCircle(u8g2, x_off + 104, 14, 4 + (pulse / 2U), U8G2_DRAW_ALL);
    u8g2_DrawDisc(u8g2, x_off + 104, 14, 2, U8G2_DRAW_ALL);
    UI_Boot_DrawProgressRail(u8g2, x_off, progress);
}

static void UI_Boot_DrawCorePulse(u8g2_t *u8g2, int16_t x_off, uint8_t progress)
{
    uint8_t i;
    uint8_t phase = (uint8_t)(g_ui_boot_frame % 12U);
    int16_t cx = x_off + 64;
    int16_t cy = 14;

    u8g2_SetDrawColor(u8g2, 1);
    for (i = 0; i < 3U; i++)
    {
        uint8_t r = (uint8_t)(4U + ((phase + i * 4U) % 12U));
        u8g2_DrawCircle(u8g2, cx, cy, r, U8G2_DRAW_ALL);
    }

    u8g2_DrawDisc(u8g2, cx, cy, 3, U8G2_DRAW_ALL);
    u8g2_DrawLine(u8g2, x_off + 12, 8, cx - 12, cy);
    u8g2_DrawLine(u8g2, x_off + 116, 8, cx + 12, cy);
    u8g2_DrawLine(u8g2, x_off + 20, 23, cx - 10, cy + 4);
    u8g2_DrawLine(u8g2, x_off + 108, 23, cx + 10, cy + 4);

    for (i = 0; i < 6U; i++)
    {
        int16_t px = x_off + 16 + (int16_t)(i * 18U);
        int16_t py = (int16_t)(6 + ((g_ui_boot_frame + i * 3U) % 16U));
        u8g2_DrawPixel(u8g2, px, py);
        u8g2_DrawPixel(u8g2, px + 1, py);
    }

    UI_Boot_DrawProgressRail(u8g2, x_off, progress);
}

static void UI_Boot_DrawPixelGather(u8g2_t *u8g2, int16_t x_off, uint8_t progress)
{
    uint8_t i;
    static const int16_t sx[12] = {4, 120, 10, 116, 28, 98, 42, 84, 2, 126, 56, 72};
    static const int16_t sy[12] = {3, 4, 27, 26, 6, 8, 28, 25, 15, 16, 2, 30};
    static const int16_t tx[12] = {48, 80, 54, 74, 59, 69, 62, 66, 52, 76, 58, 70};
    static const int16_t ty[12] = {9, 9, 20, 20, 7, 7, 22, 22, 15, 15, 14, 14};

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawFrame(u8g2, x_off + 38, 7, 52, 16);

    for (i = 0; i < 12U; i++)
    {
        int16_t x = sx[i] + (int16_t)(((int32_t)(tx[i] - sx[i]) * progress) / 100);
        int16_t y = sy[i] + (int16_t)(((int32_t)(ty[i] - sy[i]) * progress) / 100);

        u8g2_DrawBox(u8g2, x_off + x, y, 2, 2);
    }

    if (progress > 55U)
    {
        u8g2_DrawBox(u8g2, x_off + 50, 11, 28, 2);
        u8g2_DrawBox(u8g2, x_off + 50, 17, 28, 2);
    }
    if (progress > 80U)
    {
        u8g2_DrawBox(u8g2, x_off + 58, 9, 12, 12);
    }

    UI_Boot_DrawProgressRail(u8g2, x_off, progress);
}

void UI_Page_DrawBootPage(u8g2_t *u8g2, int16_t x_off)
{
    uint8_t progress = UI_Boot_GetProgress();

    switch (g_ui_boot_variant)
    {
    case 1:
        UI_Boot_DrawCorePulse(u8g2, x_off, progress);
        break;
    case 2:
        UI_Boot_DrawPixelGather(u8g2, x_off, progress);
        break;
    case 0:
    default:
        UI_Boot_DrawScanline(u8g2, x_off, progress);
        break;
    }
}

void UI_Page_DrawHomePage(u8g2_t *u8g2, int16_t x_off, uint8_t use_live_regions)
{
    if (use_live_regions)
    {
        UI_Page_DrawRegionsLive(u8g2, x_off);
    }
    else
    {
        UI_Page_DrawRegionsDefaults(u8g2, PAGE_HOME, x_off);
    }
}

void UI_Page_DrawClockPage(u8g2_t *u8g2, int16_t x_off)
{
    UI_Page_DrawRegionsLive(u8g2, x_off);
}

void UI_Page_DrawRegionsLive(u8g2_t *u8g2, int16_t x_off)
{
    uint8_t i;

    for (i = 0; i < OLED_UI_REGION_COUNT; i++)
    {
        UI_Region_t *region = &g_ui_regions[i];
        int16_t old_y;
        int16_t new_y;

        if (region->state == UI_REGION_SLIDING && region->next_draw != NULL)
        {
            if (region->slide_dir == UI_SLIDE_DOWN)
            {
                old_y = region->slide_offset;
                new_y = region->slide_offset - OLED_UI_HEIGHT;
            }
            else
            {
                old_y = -region->slide_offset;
                new_y = OLED_UI_HEIGHT - region->slide_offset;
            }

            UI_Page_DrawRegionClipped(u8g2, i, region->current_draw, x_off, old_y);
            UI_Page_DrawRegionClipped(u8g2, i, region->next_draw, x_off, new_y);
        }
        else
        {
            UI_Page_DrawRegionClipped(u8g2, i, region->current_draw, x_off, 0);
        }
    }

    u8g2_SetMaxClipWindow(u8g2);
}

void UI_Page_DrawRegionsDefaults(u8g2_t *u8g2, UI_Page_t page, int16_t x_off)
{
    uint8_t i;
    (void)page;

    for (i = 0; i < OLED_UI_REGION_COUNT; i++)
    {
        UI_Page_DrawRegionClipped(u8g2, i, g_ui_home_default_draws[i], x_off, 0);
    }

    u8g2_SetMaxClipWindow(u8g2);
}

void UI_Page_DrawRegionClipped(u8g2_t *u8g2, uint8_t idx, UI_DrawFunc_t draw, int16_t x_off, int16_t y_off)
{
    const UI_Rect_t *rect;

    if (idx >= OLED_UI_REGION_COUNT || draw == NULL)
    {
        return;
    }

    rect = &g_ui_home_regions[idx];
    u8g2_SetClipWindow(u8g2,
                       (u8g2_uint_t)(rect->x + x_off),
                       (u8g2_uint_t)rect->y,
                       (u8g2_uint_t)(rect->x + rect->w + x_off),
                       (u8g2_uint_t)(rect->y + rect->h));
    draw(u8g2, rect->x + x_off, rect->y + y_off, rect->w, rect->h);
    u8g2_SetMaxClipWindow(u8g2);
}
