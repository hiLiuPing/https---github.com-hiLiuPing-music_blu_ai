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


// #include "uart_dma.h"

extern SemaphoreHandle_t xAppDataTaskWakeSemaphore;
extern volatile UI_Global_t g_ui;

#define APPDATA_QUOTE_WAKE_SETTLE_MS      200U
#define APPDATA_LOW_BATTERY_COOLDOWN_MS   (180U * 1000U)

static TickType_t s_last_low_battery_tick = 0U;

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
            // log_printf("number==%d,numberB==%d",number,numberB);
            /* 同步 UI 电池百分比 */
            {
                float s = g_sensors_battery.soc;
                uint8_t pct;

                if (s < 0.0f) s = 0.0f;
                else if (s > 100.0f) s = 100.0f;
                pct = (uint8_t)(s + 0.5f);
                g_ui.battery.percent = pct;
            }

            /* 低电量检测：< 30% 且不在充电，180 秒防抖 */
            if (g_sensors_battery.soc < 30.0f && !g_ui.battery.is_charging)
            {
                if ((now - s_last_low_battery_tick) >= pdMS_TO_TICKS(APPDATA_LOW_BATTERY_COOLDOWN_MS))
                {
                    s_last_low_battery_tick = now;
                    (void)OLED_UI_PostEvent(UI_EVT_BATTERY_LOW, "AppData");
                }
            }
              
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
            music_send_cmd(CMD_POWER_OFF);
            vTaskDelay(pdMS_TO_TICKS(50));
            music_send_cmd(CMD_SYSTEM_POWER_OFF);
            (void)OLED_UI_PostEvent(UI_EVT_KEY_TIMEOUT, "AppData");
            log_printf("Key idle timeout.\r\n");
        }
        if (g_bulu_timeout)
        {
            g_bulu_timeout = 0;
            // music_send_cmd(CMD_POWER_OFF);
            music_send_cmd(CMD_SYSTEM_POWER_OFF);
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
