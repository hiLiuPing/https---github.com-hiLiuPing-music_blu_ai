#include "../ui_widget.h"
#include "sensors_app.h"

static uint8_t UI_Widget_BatterySocPercent(void)
{
    float soc = g_sensors_battery.soc;

    if (soc < 0.0f)
    {
        soc = 0.0f;
    }
    else if (soc > 100.0f)
    {
        soc = 100.0f;
    }

    return (uint8_t)(soc + 0.5f);
}

/* 电池与供电相关 widget。 */
void UI_Widget_DrawHomeDefaultRight(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    uint8_t pct = UI_Widget_BatterySocPercent();

    (void)w;
    (void)h;
    UI_Widget_DrawStatusBattery(u8g2, x + 6, y + 7, pct, g_ui.battery.is_charging);
    u8g2_SetFont(u8g2, u8g2_font_5x8_tf);
    u8g2_DrawStr(u8g2, x + 6, y + 28, g_ui.battery.batterypower ? "USB" : "BAT");
}

void UI_Widget_DrawRegionBattery(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    char buf[8];
    uint8_t pct = UI_Widget_BatterySocPercent();

    (void)h;

    snprintf(buf, sizeof(buf), "%u%%", pct);
    UI_Widget_DrawStatusBattery(u8g2, x + 4, y + 7, pct, g_ui.battery.is_charging);
    u8g2_SetFont(u8g2, u8g2_font_6x10_tf);
    UI_Widget_DrawCenteredStr(u8g2, x, y + 30, w, buf);
}

void UI_Widget_DrawRegionPower(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    (void)h;
    u8g2_SetFont(u8g2, u8g2_font_6x10_tf);
    UI_Widget_DrawCenteredStr(u8g2, x, y + 13, w, "PWR");
    UI_Widget_DrawCenteredStr(u8g2, x, y + 27, w, g_ui.battery.batterypower ? "IN" : "OUT");
}
