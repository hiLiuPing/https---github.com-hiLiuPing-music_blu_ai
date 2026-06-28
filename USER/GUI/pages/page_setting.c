#include "../ui_page.h"

/* 设置/第二页面绘制。 */
void UI_Page_DrawDynamicPage2(u8g2_t *u8g2, int16_t x_off, uint8_t use_live_regions)
{
    if (use_live_regions)
    {
        UI_Page_DrawRegionsLive(u8g2, x_off);
        return;
    }

    UI_Page_DrawRegionsDefaults(u8g2, PAGE_PAGE2, x_off);
}
