#include "user_TasksInit.h"
#include "user_OLEDTask.h"
#include "main.h"
#include "oled_ui.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "log.h"
#include "systemMonitor_app.h"

/*
 * OLED refresh task.
 * This is the high-frequency UI loop while the screen is on.
 */
extern SemaphoreHandle_t xOLedShowTaskWakeSemaphore;
extern volatile UI_Global_t g_ui;

void OLEDTask(void *arg)
{
    TickType_t last_wake_time = 0;
    uint8_t pending_sleep = 0U;
    UI_Event_t evt;

    (void)arg;

    /* Leave a startup window for hardware init so OLED/UI is ready first. */
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Seed an initial battery state to avoid a blank battery area on boot. */
    OLED_UI_SetBattery(60, 0);

    while (1)
    {
        if (!g_ui.sys_running)
        {
            xSemaphoreTake(xOLedShowTaskWakeSemaphore, portMAX_DELAY);
            last_wake_time = 0U;
            pending_sleep = 0U;
            MemDiag_LogSnapshot("oled-wakeup");
        }

        while (xQueueReceive(OLED_UI_queue, &evt, 0) == pdPASS)
        {
            if (evt == UI_EVT_SLEEP_REQUEST)
            {
                pending_sleep = 1U;
                continue;
            }

            if (evt == UI_EVT_SHOW_ON)
            {
                pending_sleep = 0U;

                if (g_ui.oled_showing != 0U)
                {
                    Monitor_Restart(MON_OLED_IDLE);
                    continue;
                }
            }

            OLED_UI_OnEvent(evt);

            if (evt == UI_EVT_SHOW_ON)
            {
                Monitor_Restart(MON_OLED_IDLE);
            }
        }

        if (pending_sleep)
        {
            pending_sleep = 0U;
            g_ui.oled_showing = 0U;
            OLED_Sleep();
            g_ui.sys_running = 0U;
            last_wake_time = 0U;
            MemDiag_LogSnapshot("oled-sleep");
            continue;
        }

        OLED_UI_Update();
        OLED_UI_Render();

        /* Refresh at a stable 30ms cadence while the panel is on. */
        if (last_wake_time == 0U)
        {
            last_wake_time = xTaskGetTickCount();
        }
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(30));
    }
}
