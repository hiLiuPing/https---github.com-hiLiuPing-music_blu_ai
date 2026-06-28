#include "ui_page.h"

/*
 * The page layer decides what to draw.
 * Navigation timing and transitions stay in ui_nav.c.
 */

static const UI_ExtPageConfig_t s_ui_ext_pages[] = {
    {PAGE_PAGE1, UI_EXT_STATIC},
    {PAGE_PAGE2, UI_EXT_DYNAMIC},
};

UI_ExtPageMode_t UI_Page_GetExtMode(UI_Page_t page)
{
    uint8_t i;

    for (i = 0; i < (uint8_t)(sizeof(s_ui_ext_pages) / sizeof(s_ui_ext_pages[0])); i++)
    {
        if (s_ui_ext_pages[i].page == page)
        {
            return s_ui_ext_pages[i].mode;
        }
    }

    return UI_EXT_STATIC;
}

uint8_t UI_Page_IsDynamic(UI_Page_t page)
{
    if (page == PAGE_HOME)
    {
        return 1U;
    }

    return (UI_Page_GetExtMode(page) == UI_EXT_DYNAMIC) ? 1U : 0U;
}

uint8_t UI_Page_CanUsePopups(UI_Page_t page)
{
    return (page != PAGE_BOOT) ? 1U : 0U;
}

void UI_Page_DrawAt(u8g2_t *u8g2, UI_Page_t page, int16_t x_off)
{
    switch (page)
    {
    case PAGE_BOOT:
        UI_Page_DrawBootPage(u8g2, x_off);
        break;
    case PAGE_HOME:
        UI_Page_DrawHomePage(u8g2, x_off, (page == g_ui.cur_page && !g_ui_page_trans.active) ? 1U : 0U);
        break;
    case PAGE_CLOCK:
        UI_Page_DrawClockPage(u8g2, x_off);
        break;
    default:
        break;
    }
}

void UI_Page_DrawSwitch(u8g2_t *u8g2)
{
    uint8_t progress = (uint8_t)(-g_ui_page_trans.offset);

    if (progress < (OLED_UI_WIDTH / 2U))
    {
        UI_Page_DrawAt(u8g2, g_ui_page_trans.from_page, 0);
        u8g2_SetDrawColor(u8g2, 1);
        u8g2_DrawBox(u8g2, 0, 0, progress * 2U, OLED_UI_HEIGHT);
        u8g2_SetDrawColor(u8g2, 0);
        u8g2_DrawBox(u8g2, 0, 0, progress, OLED_UI_HEIGHT);
        u8g2_SetDrawColor(u8g2, 1);
    }
    else
    {
        uint8_t reveal = (uint8_t)((progress - (OLED_UI_WIDTH / 2U)) * 2U);

        UI_Page_DrawAt(u8g2, g_ui_page_trans.to_page, 0);
        u8g2_SetDrawColor(u8g2, 0);
        u8g2_DrawBox(u8g2, reveal, 0, OLED_UI_WIDTH - reveal, OLED_UI_HEIGHT);
        u8g2_SetDrawColor(u8g2, 1);
    }
}

void UI_Page_DrawBaseLayer(u8g2_t *u8g2)
{
    if (g_ui_page_trans.active)
    {
        UI_Page_DrawSwitch(u8g2);
    }
    else
    {
        UI_Page_DrawAt(u8g2, g_ui.cur_page, 0);
    }
}
