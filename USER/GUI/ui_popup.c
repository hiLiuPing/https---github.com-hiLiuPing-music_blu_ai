#include <stdint.h>
#include <string.h>

#include "data_app.h"
#include "ui_popup.h"

/*
 * 语录弹窗渲染层。
 * 普通系统弹窗和语录弹窗都从这里输出到最上层。
 */

#define QUOTE_BOX_X                  2
#define QUOTE_BOX_W                  124
#define QUOTE_BOX_H                  24
#define QUOTE_BOX_Y_OFFSET           4
#define QUOTE_TEXT_X                 (QUOTE_BOX_X + 4)
#define QUOTE_TEXT_Y_OFFSET          8

static DataApp_QuotePopupFrame_t s_quote_popup_frame;

static void Draw_QuotePopup(u8g2_t *u8g2, UI_Comp_t *base, void *data);
static uint8_t Quote_GetPixel(const DataApp_QuotePopupFrame_t *frame, uint16_t x, uint8_t y);
static void Quote_DrawPreparedFrame(u8g2_t *u8g2, UI_Comp_t *base, const DataApp_QuotePopupFrame_t *frame);

void Draw_SystemMsgPopup(u8g2_t *u8g2, UI_Comp_t *base, void *data)
{
    char *msg = (char *)data;
    int y_pos;
    int str_w;
    int box_w;
    int box_x;
    int box_y;

    if (msg == NULL)
    {
        return;
    }

    y_pos = (int)base->y;
    u8g2_SetFont(u8g2, u8g2_font_6x12_tf);

    str_w = (int)u8g2_GetStrWidth(u8g2, msg);
    box_w = str_w + 16;
    box_x = (OLED_UI_WIDTH - box_w) / 2;
    box_y = y_pos + 6;

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawRBox(u8g2, box_x, box_y, box_w, 18, 4);

    u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawStr(u8g2, (OLED_UI_WIDTH - str_w) / 2, y_pos + 19, msg);
    u8g2_SetDrawColor(u8g2, 1);
}

void Draw_BatteryFullPopup(u8g2_t *u8g2, UI_Comp_t *base, void *data)
{
    int y = (int)base->y;
    int flow;

    (void)data;

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawBox(u8g2, 0, y, OLED_UI_WIDTH, OLED_UI_HEIGHT);

    u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawFrame(u8g2, 10, y + 2, 108, 28);
    u8g2_DrawBox(u8g2, 118, y + 8, 6, 16);

    flow = (int)((xTaskGetTickCount() / 150U) % 21U);
    if (flow > 0)
    {
        u8g2_DrawBox(u8g2, 14, y + 6, flow * 5, 20);
    }

    u8g2_SetDrawColor(u8g2, 1);
}

void UI_Popup_DrawTopLayer(u8g2_t *u8g2)
{
    if (!g_ui_page_trans.active && UI_Page_CanUsePopups(g_ui.cur_page))
    {
        memset(&s_quote_popup_frame, 0, sizeof(s_quote_popup_frame));
        (void)DataApp_QuotePopup_CopyFrame(&s_quote_popup_frame);
        UI_Popup_SetLowRuntimeData(&s_quote_popup_frame);
        UI_Popup_Render(u8g2);
    }
}

uint8_t UI_Popup_EnqueueQuotePopup(const char *msg, uint32_t ms)
{
    return UI_Popup_ShowLow(Draw_QuotePopup, msg, ms);
}

static uint8_t Quote_GetPixel(const DataApp_QuotePopupFrame_t *frame, uint16_t x, uint8_t y)
{
    uint16_t byte_index;
    uint8_t mask;

    if (frame == NULL || x >= frame->bitmap_width || y >= DATA_APP_QUOTE_FONT_H)
    {
        return 0U;
    }

    byte_index = (uint16_t)(y * DATA_APP_QUOTE_RENDER_STRIDE) + (uint16_t)(x / 8U);
    mask = (uint8_t)(0x80U >> (x & 0x07U));

    return ((frame->bitmap[byte_index] & mask) != 0U) ? 1U : 0U;
}

static void Quote_DrawPreparedFrame(u8g2_t *u8g2, UI_Comp_t *base, const DataApp_QuotePopupFrame_t *frame)
{
    int box_y;
    int text_y;
    int text_base_x;
    uint16_t scroll_px;
    uint16_t visible_w;
    uint8_t row;
    uint16_t col;

    if (frame == NULL || !frame->valid || frame->bitmap_width == 0U)
    {
        return;
    }

    box_y = (int)base->y + QUOTE_BOX_Y_OFFSET;
    text_y = (int)base->y + QUOTE_TEXT_Y_OFFSET;

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawRBox(u8g2, QUOTE_BOX_X, box_y, QUOTE_BOX_W, QUOTE_BOX_H, 4);
    u8g2_SetDrawColor(u8g2, 0);

    text_base_x = QUOTE_TEXT_X - (int)frame->visible_offset_x;

    if (frame->bitmap_width <= DATA_APP_QUOTE_VIEW_W)
    {
        scroll_px = 0U;
        visible_w = frame->bitmap_width;
    }
    else
    {
        scroll_px = frame->scroll_px;
        if (scroll_px > frame->scroll_limit)
        {
            scroll_px = frame->scroll_limit;
        }
        if (scroll_px < frame->bitmap_width)
        {
            visible_w = (uint16_t)(frame->bitmap_width - scroll_px);
            if (visible_w > DATA_APP_QUOTE_VIEW_W)
            {
                visible_w = DATA_APP_QUOTE_VIEW_W;
            }
        }
        else
        {
            visible_w = 0U;
        }
    }

    for (row = 0; row < DATA_APP_QUOTE_FONT_H; row++)
    {
        int16_t dst_y = (int16_t)text_y + row;

        if (dst_y < 0 || dst_y >= (int16_t)OLED_UI_HEIGHT)
        {
            continue;
        }

        for (col = 0; col < visible_w; col++)
        {
            uint16_t src_x = (uint16_t)(scroll_px + col);
            int16_t dst_x;

            if (src_x >= frame->bitmap_width)
            {
                continue;
            }

            dst_x = (int16_t)text_base_x + (int16_t)col;

            if (dst_x < 0 || dst_x >= (int16_t)OLED_UI_WIDTH)
            {
                continue;
            }

            if (Quote_GetPixel(frame, src_x, row))
            {
                u8g2_DrawPixel(u8g2, (u8g2_uint_t)dst_x, (u8g2_uint_t)dst_y);
            }
        }
    }

    u8g2_SetDrawColor(u8g2, 1);
}

static void Draw_QuotePopup(u8g2_t *u8g2, UI_Comp_t *base, void *data)
{
    const DataApp_QuotePopupFrame_t *frame = (const DataApp_QuotePopupFrame_t *)data;

    if (frame == NULL || frame->valid == 0U)
    {
        return;
    }

    Quote_DrawPreparedFrame(u8g2, base, frame);
}
