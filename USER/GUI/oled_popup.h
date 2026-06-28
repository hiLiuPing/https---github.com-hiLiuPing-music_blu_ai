#ifndef __OLED_POPUP_H__
#define __OLED_POPUP_H__

/*
 * 弹窗队列与状态机。
 * 负责把一个弹窗从“请求”变成“显示中”，并自动推进生命周期。
 */

#include "FreeRTOS.h"
#include "task.h"
#include "oled_anim.h"
#include "oled_u8g2.h"

#define POPUP_MSG_BUF_LEN 192

typedef void (*PopupDrawFunc)(u8g2_t *u8g2, UI_Comp_t *base, void *data);

typedef struct {
    PopupDrawFunc draw_cb;
    const void *data;
    uint32_t ms;
} PopupTask_t;

typedef struct {
    UI_Comp_t base;
    PopupDrawFunc draw_cb;
    char msg_buf[POPUP_MSG_BUF_LEN];
    uint32_t timer;
    TickType_t start_tick;
    uint8_t is_active;
} UI_Popup_t;

void UI_Popup_Init(void);
uint8_t UI_Popup_Show(PopupDrawFunc draw_cb, const char *msg, uint32_t ms);
void UI_Popup_Update(void);
void UI_Popup_Render(u8g2_t *u8g2);
uint8_t UI_Popup_IsBusy(void);
uint8_t UI_Popup_IsShowing(void);
uint32_t UI_Popup_GetElapsedMs(void);

/* 低优先级弹窗（语录等，不与系统弹窗争抢显示槽位）。 */
uint8_t UI_Popup_ShowLow(PopupDrawFunc draw_cb, const char *msg, uint32_t ms);
uint8_t UI_Popup_IsBusyLow(void);
uint8_t UI_Popup_IsLowShowing(void);
void UI_Popup_SetLowRuntimeData(const void *data);

#endif
