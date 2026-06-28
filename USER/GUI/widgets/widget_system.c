#include "../ui_widget.h"

/* 系统信息 widget。 */
void UI_Widget_DrawRegionAir(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    char buf[8];
    (void)h;
    u8g2_SetFont(u8g2, u8g2_font_5x8_tf);
    snprintf(buf, sizeof(buf), "%d", g_air_detail.aqi);
    UI_Widget_DrawCenteredStr(u8g2, x, y + 10, w, "AIR");
    UI_Widget_DrawCenteredWeatherStr(u8g2, x, y + 27, w, buf);
}

void UI_Widget_DrawRegionSystem(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    (void)h;
    u8g2_SetFont(u8g2, u8g2_font_5x8_tf);
    UI_Widget_DrawCenteredStr(u8g2, x, y + 13, w, "SYS");
    UI_Widget_DrawCenteredStr(u8g2, x, y + 27, w, "OK");
}
