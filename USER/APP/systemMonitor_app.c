/*============================================================
 * File: SystemMonitor_app.c
 *===========================================================*/
#include "systemMonitor_app.h"

#include <stddef.h>

#include "weather_app.h"
#include "data_app.h"
#include "log.h"
#include "oled_ui.h"
#include "lptim.h"
#include "task.h"
#include "timers.h"
#include "user_TasksInit.h"

extern QueueHandle_t OLED_UI_queue;
/*
 * App-level monitor layer.
 * Keeps the remaining timeout actions behind a small shared facade.
 */

static const char *OLED_UI_EventName(UI_Event_t evt)
{
    switch (evt)
    {
    case UI_EVT_SHOW_ON:
        return "SHOW_ON";
    case UI_EVT_SHOW_OFF:
        return "SHOW_OFF";
    case UI_EVT_SLEEP_REQUEST:
        return "SLEEP_REQ";
    case UI_EVT_SYS_RUNNING:
        return "SYS_RUN";
    case UI_EVT_SYS_STOP:
        return "SYS_STOP";
    case UI_EVT_WEATHER_TIME_SYNC:
        return "WEATHER_SYNC";
    case UI_EVT_KEY_TIMEOUT:
        return "KEY_TIMEOUT";
    case UI_EVT_BULU_TIMEOUT:
        return "BULU_TIMEOUT";
    case UI_EVT_MUSIC_TIMEOUT:
        return "MUSIC_TIMEOUT";
    default:
        return "UI_EVT";
    }
}

static uint8_t OLED_UI_IsStateEvent(UI_Event_t evt)
{
    return (evt == UI_EVT_SHOW_ON || evt == UI_EVT_SLEEP_REQUEST) ? 1U : 0U;
}

uint8_t OLED_UI_PostEvent(UI_Event_t evt, const char *source)
{
    BaseType_t status;

    if (OLED_UI_queue == NULL)
    {
        log_printf("[OLEDQ] drop %s src=%s queue=null\r\n",
                   OLED_UI_EventName(evt),
                   (source != NULL) ? source : "?");
        return 0U;
    }

    status = xQueueSend(OLED_UI_queue, &evt, 0);
    if (status != pdPASS)
    {
        log_printf("[OLEDQ] drop %s src=%s full=%lu\r\n",
                   OLED_UI_EventName(evt),
                   (source != NULL) ? source : "?",
                   (unsigned long)uxQueueMessagesWaiting(OLED_UI_queue));
        return 0U;
    }

    return 1U;
}

uint8_t OLED_UI_PostStateEvent(UI_Event_t evt, const char *source)
{
    if (!OLED_UI_IsStateEvent(evt))
    {
        return OLED_UI_PostEvent(evt, source);
    }

    if (OLED_UI_PostEvent(evt, source))
    {
        return 1U;
    }

    if (OLED_UI_queue == NULL)
    {
        return 0U;
    }

    log_printf("[OLEDQ] reset for %s src=%s\r\n",
               OLED_UI_EventName(evt),
               (source != NULL) ? source : "?");
    xQueueReset(OLED_UI_queue);

    if (xQueueSend(OLED_UI_queue, &evt, 0) != pdPASS)
    {
        log_printf("[OLEDQ] retry fail %s src=%s\r\n",
                   OLED_UI_EventName(evt),
                   (source != NULL) ? source : "?");
        return 0U;
    }

    return 1U;
}

static void OLEDIdleTimeout(TimerHandle_t xTimer)
{
    (void)xTimer;
    log_printf("[OLED] idle timeout\r\n");
    (void)OLED_UI_PostStateEvent(UI_EVT_SLEEP_REQUEST, "Monitor");
}

/**
 * @brief  Initialize app-level monitor items
 */
void UserMonitor_Init(void)
{
    Monitor_CreateEx(MON_OLED_IDLE, "OLED_IDLE", 30000, 1, OLEDIdleTimeout);
}

/**
 * @brief  Any input event counts as activity
 */
void Key_Event(void)
{
    LPTIM_ResetKeyIdle();
    if (g_ui.oled_showing)
    {
        Monitor_Restart(MON_OLED_IDLE);
    }
}

void MemDiag_LogSnapshot(const char *tag)
{
    size_t free_heap;
    size_t min_free_heap;
    BaseType_t scheduler_state;

    free_heap = xPortGetFreeHeapSize();
    min_free_heap = xPortGetMinimumEverFreeHeapSize();
    scheduler_state = xTaskGetSchedulerState();

    log_printf("[MemDiag] ===== %s =====\r\n", (tag != NULL) ? tag : "snapshot");
    log_printf("[MemDiag] heap_free=%luB heap_min=%luB\r\n",
               (unsigned long)free_heap,
               (unsigned long)min_free_heap);
    if (scheduler_state == taskSCHEDULER_NOT_STARTED)
    {
        log_printf("[MemDiag] scheduler=not-started\r\n");
        return;
    }
}
