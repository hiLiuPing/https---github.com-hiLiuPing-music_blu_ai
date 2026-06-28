#ifndef __SYSTEMMONITOR_APP_H
#define __SYSTEMMONITOR_APP_H

/*
 * System monitor / timeout management layer.
 * Handles key, OLED, and weather monitor items still kept on Monitor_CreateEx.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "oled_ui.h"
#include "systemMonitor.h"

/* Monitor item IDs. */
typedef enum
{
    MON_KEY_IDLE = 0,
    MON_OLED_IDLE,
    MON_APP_MAX
} AppMonitorID_t;

/* Initialize all monitor items and timeout callbacks. */
void UserMonitor_Init(void);

/* Any key activity counts as active. */
void Key_Event(void);
uint8_t OLED_UI_PostEvent(UI_Event_t evt, const char *source);
uint8_t OLED_UI_PostStateEvent(UI_Event_t evt, const char *source);
void MemDiag_LogSnapshot(const char *tag);

#ifdef __cplusplus
}
#endif

#endif
