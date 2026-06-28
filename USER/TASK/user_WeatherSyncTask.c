/* Private includes -----------------------------------------------------------*/
// includes
#include "user_TasksInit.h"
#include "user_WeatherSyncTask.h"
#include "weather_app.h"
#include "systemMonitor_app.h"
#include "FreeRTOS.h"
#include "task.h"
#include "oled_ui.h"
#include "log.h"

extern SemaphoreHandle_t xWeatherSyncTaskWakeSemaphore;
extern volatile UI_Global_t g_ui;

static void Weather_RunSingleSyncRound(void)
{
    Weather_BeginSyncCycle();
    Weather_SendCommand(CMD_GET_TIME);        vTaskDelay(pdMS_TO_TICKS(200));
    Weather_SendCommand(CMD_GET_NOW_DETAIL);  vTaskDelay(pdMS_TO_TICKS(200));
    Weather_SendCommand(CMD_GET_AIR_DETAIL);  vTaskDelay(pdMS_TO_TICKS(200));
    Weather_SendCommand(CMD_GET_FUTURE_7DAY); vTaskDelay(pdMS_TO_TICKS(200));
}

static uint8_t Weather_RunSyncWithRetry(const char *success_log,
                                        const char *fail_log)
{
    uint8_t retry;

    for (retry = 0U; retry < 15U; retry++)
    {
        if (!g_ui.sys_running)
        {
            break;
        }

        Weather_RunSingleSyncRound();
        if (Weather_HasCompletedSync())
        {
            log_printf(success_log, retry + 1U);
            return 1U;
        }
    }

    if (g_ui.sys_running)
    {
        log_printf(fail_log);
    }

    return 0U;
}

void WeatherSyncTask(void *arg)
{
    (void)arg;

    vTaskDelay(pdMS_TO_TICKS(1000));

    for (;;)
    {
        if (!g_ui.sys_running)
        {
            xSemaphoreTake(xWeatherSyncTaskWakeSemaphore, portMAX_DELAY);
            xTaskNotifyStateClear(NULL);
            (void)ulTaskNotifyTake(pdTRUE, 0);
        }

        if (g_weather_module.first_sync_done == 0U)
        {
            uint8_t sync_success;

            Weather_PowerOn();
            sync_success = Weather_RunSyncWithRetry("[Weather] first sync success, retry=%u\r\n",
                                                    "[Weather] first sync total timeout! Giving up, waiting for shake trigger...\r\n");

            Weather_PowerOff();

            if (!g_ui.sys_running)
            {
                log_printf("[Weather] first sync interrupted by system sleep.\r\n");
            }
            else
            {
                if (sync_success != 0U)
                {
                    g_weather_module.first_sync_done = 1U;
                }
            }

            continue;
        }

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (g_weather_module.syncing)
        {
            continue;
        }

        g_weather_module.syncing = 1U;

        Weather_PowerOn();
        vTaskDelay(pdMS_TO_TICKS(3000));

        if (g_ui.sys_running)
        {
            if (Weather_RunSyncWithRetry("[Weather] sync success, retry=%u\r\n",
                                         "[Weather] sync total timeout after 15 retries\r\n") != 0U)
            {
                log_printf("[Weather] sync done\r\n");
            }
        }
         Weather_PowerOff();

        g_weather_module.syncing = 0U;
    }
}
