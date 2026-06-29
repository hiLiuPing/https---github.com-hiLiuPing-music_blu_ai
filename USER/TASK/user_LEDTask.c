/* Private includes -----------------------------------------------------------*/
// includes
#include "user_TasksInit.h"
#include "user_LEDTask.h"
#include "main.h"
#include "oled_ui.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "led_app.h"
#include "log.h"

/*
 * LED refresh task.
 * Keep the effect engine alive, but slow it down when the OLED is off.
 */

// 假设在头文件或本文件上方声明
extern SemaphoreHandle_t xLedTaskWakeSemaphore; 
extern volatile UI_Global_t g_ui;

extern RGB_Object_t rgb;

void LEDTask(void *pvParameters)
{
    (void)pvParameters;

    TickType_t led_scan_tick_count = xTaskGetTickCount();

    LED_EVT_t cmd;
    while (1)
    {

        if (!g_ui.sys_running)
        {
            // log_printf("ledTask Sleep/Suspended...\r\n");
            
            // 无限期等待信号量，直到被外部中断或系统控制逻辑唤醒
            xSemaphoreTake(xLedTaskWakeSemaphore, portMAX_DELAY);

        }
       if (xQueueReceive(LED_cmd_queue, &cmd, 0) == pdPASS)
        {
            LED_CMD_OnEvent(cmd);
        }
        LED_Driver_Update();
        RGB_Update(&rgb, pdMS_TO_TICKS(10));

        vTaskDelay(10);
    }
}
