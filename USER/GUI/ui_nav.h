#ifndef __UI_NAV_H__
#define __UI_NAV_H__

/* UI 导航/调度层：Boot、切页、动态区域、粒子和弹窗推进都在这里。 */

#include "ui_data.h"

void UI_Nav_Init(void);
void UI_Nav_Update(void);
void UI_Nav_RequestPage(UI_Page_t page);
void UI_Nav_WakeUp(void);
void UI_Nav_DrawParticleLayer(u8g2_t *u8g2);
void UI_Nav_DrawShutdownOverlay(u8g2_t *u8g2);
void UI_Nav_ShutdownStart(void);
void UI_Nav_ShutdownKeepAlive(void);
void UI_Nav_ShutdownCancel(void);
uint8_t UI_Nav_ShutdownActive(void);

#endif
