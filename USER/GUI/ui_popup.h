#ifndef __UI_POPUP_H__
#define __UI_POPUP_H__

/* UI 顶层弹窗接口。 */

#include "ui_data.h"
#include "ui_page.h"

void Draw_SystemMsgPopup(u8g2_t *u8g2, UI_Comp_t *base, void *data);
void Draw_BatteryFullPopup(u8g2_t *u8g2, UI_Comp_t *base, void *data);
void UI_Popup_DrawTopLayer(u8g2_t *u8g2);
uint8_t UI_Popup_EnqueueQuotePopup(const char *msg, uint32_t ms);

#endif
