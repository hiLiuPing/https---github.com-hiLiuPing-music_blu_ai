#ifndef __UI_PAGE_H__
#define __UI_PAGE_H__

/*
 * 页面层接口。
 * 负责“画什么”，不负责“什么时候切换”。
 */

#include "ui_data.h"

UI_ExtPageMode_t UI_Page_GetExtMode(UI_Page_t page);
uint8_t UI_Page_IsDynamic(UI_Page_t page);
uint8_t UI_Page_CanUsePopups(UI_Page_t page);

void UI_Page_DrawAt(u8g2_t *u8g2, UI_Page_t page, int16_t x_off);
void UI_Page_DrawBaseLayer(u8g2_t *u8g2);
void UI_Page_DrawSwitch(u8g2_t *u8g2);

void UI_Page_DrawBootPage(u8g2_t *u8g2, int16_t x_off);
void UI_Page_DrawHomePage(u8g2_t *u8g2, int16_t x_off, uint8_t use_live_regions);
void UI_Page_DrawClockPage(u8g2_t *u8g2, int16_t x_off);
void UI_Page_DrawStaticPage1(u8g2_t *u8g2, int16_t x_off);
void UI_Page_DrawDynamicPage2(u8g2_t *u8g2, int16_t x_off, uint8_t use_live_regions);
void UI_Page_DrawRegionsLive(u8g2_t *u8g2, int16_t x_off);
void UI_Page_DrawRegionsDefaults(u8g2_t *u8g2, UI_Page_t page, int16_t x_off);
void UI_Page_DrawRegionClipped(u8g2_t *u8g2, uint8_t idx, UI_DrawFunc_t draw, int16_t x_off, int16_t y_off);

#endif
