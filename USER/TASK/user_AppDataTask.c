/* Private includes -----------------------------------------------------------*/
// includes
#include "user_TasksInit.h"
#include "user_AppDataTask.h"
#include "main.h"
#include "oled_ui.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "data_app.h"
#include "sensors_app.h"
#include "systemMonitor_app.h"
#include "lptim.h"
#include "ui_popup.h"
#include "log.h"
#include "music_app.h"
#include "systemMonitor_app.h"
extern SemaphoreHandle_t xAppDataTaskWakeSemaphore;
extern volatile UI_Global_t g_ui;

#define APPDATA_QUOTE_WAKE_SETTLE_MS      200U
/*
 * Application data task.
 * Runs fast when the OLED is on, and slows down when the panel is off.
 */


void AppDataTask(void *argument)
{
    (void)argument;

    vTaskDelay(pdMS_TO_TICKS(1000));
    TickType_t last_wake_time = xTaskGetTickCount();
    TickType_t last_1s_tick = xTaskGetTickCount();
    const TickType_t ms_100 = pdMS_TO_TICKS(1000);
  
    log_printf("[AppData] quote timer started\r\n");


        //    if (xAppDataTaskWakeSemaphore != NULL)
        // {
        //     xSemaphoreGive(xAppDataTaskWakeSemaphore);
        // }

    for (;;)
    {

        if (!g_ui.sys_running)
        {
            // log_printf("appdata Sleep/Suspended...\r\n");
            
            // 无限期等待信号量，直到被外部中断或系统控制逻辑唤醒
            xSemaphoreTake(xAppDataTaskWakeSemaphore, portMAX_DELAY);
            last_wake_time = xTaskGetTickCount();
            last_1s_tick = last_wake_time;
        }

        TickType_t now = xTaskGetTickCount();
        Update_Motion(&g_sensors_motion);
        Motion_SwapBuffer(&g_sensors_motion);

        DataApp_QuoteServiceUpdate(now);

        {
            uint32_t notify_cnt = ulTaskNotifyTake(pdTRUE, 0);

            while (notify_cnt-- != 0U)
            {
                DataApp_QuotePopupRequest_t req;

                if (DataApp_QuoteShowNext(&req))
                {
                    if (!g_ui.oled_showing)
                    {
                        vTaskDelay(pdMS_TO_TICKS(APPDATA_QUOTE_WAKE_SETTLE_MS));
                    }

                    UI_Popup_EnqueueQuotePopup(req.text, req.duration_ms);
                }

            }
            g_quote_ready = 0;
        }

        if ((now - last_1s_tick) >= ms_100)
        {
            last_1s_tick += ms_100;
            Update_Env(&g_sensors_environment);
            Update_Battery(&g_sensors_battery);

            if (g_ui.oled_showing != 0U)
            {
                RTC_ReadToBuffer();
                Buffer_Swap();
                // Time_BlinkUpdate();
            }
        }

        if (g_key_idle_timeout)
        {
            g_key_idle_timeout = 0;
            // System_PowerOff();
            music_send_cmd(CMD_SYSTEM_POWER_OFF);
            (void)OLED_UI_PostEvent(UI_EVT_KEY_TIMEOUT, "AppData");
            log_printf("Key idle timeout.\r\n");
        }
        if (g_bulu_timeout)
        {
            g_bulu_timeout = 0;
            music_send_cmd(CMD_POWER_OFF);
            (void)OLED_UI_PostEvent(UI_EVT_BULU_TIMEOUT, "AppData");
            log_printf("BULU timeout.\r\n");
        }
        if (g_music_timeout)
        {
            g_music_timeout = 0;
            music_send_cmd(CMD_POWER_OFF);
            (void)OLED_UI_PostEvent(UI_EVT_MUSIC_TIMEOUT, "AppData");
            log_printf("MUSCI timeout.\r\n");
        }

        if (g_ui.sys_running)
        {
            vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(30));
        }

    }
}
