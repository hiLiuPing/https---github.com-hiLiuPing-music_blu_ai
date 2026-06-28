#ifndef __UI_MSG_H__
#define __UI_MSG_H__

/* UI 事件映射层：把业务事件翻译成弹窗和页面动作。 */

#include "oled_ui.h"

void UI_Msg_SetBattery(uint8_t percent, uint8_t charging);
void UI_Msg_HandleEvent(UI_Event_t event);

#endif
