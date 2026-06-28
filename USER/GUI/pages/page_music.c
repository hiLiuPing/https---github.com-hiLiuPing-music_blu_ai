#include "../ui_page.h"

/* 音乐页绘制。 */
void UI_Page_DrawStaticPage1(u8g2_t *u8g2, int16_t x_off)
{
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawFrame(u8g2, x_off + 2, 2, 124, 28);
    u8g2_DrawTriangle(u8g2, x_off + 16, 10, x_off + 16, 22, x_off + 27, 16);

    u8g2_SetFont(u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(u8g2, x_off + 38, 12, "PAGE1 MUSIC");
    u8g2_DrawStr(u8g2, x_off + 38, 24, g_music_ble_state.ble_connected ? "BT OK" : "BT WAIT");

    if (g_music_ble_state.music_played)
    {
        u8g2_DrawBox(u8g2, x_off + 104, 10, 4, 12);
        u8g2_DrawBox(u8g2, x_off + 112, 10, 4, 12);
    }
    else
    {
        u8g2_DrawStr(u8g2, x_off + 100, 21, "II");
    }
}
