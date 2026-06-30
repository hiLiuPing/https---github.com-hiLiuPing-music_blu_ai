/* Private includes -----------------------------------------------------------*/
// includes
#include "user_TasksInit.h"
#include "user_KeyTask.h"
#include "key.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "data_app.h"
#include "log.h"
#include "ui_data.h"

// 澹版槑鍏ㄥ眬鎴栭潤鎬佷俊鍙烽噺锛堥渶鍦ㄥ垵濮嬪寲鏃?xSemaphoreCreateBinary()锛?
extern SemaphoreHandle_t xKeyScanTaskWakeSemaphore;
extern volatile UI_Global_t g_ui;

/*
 * 杈撳叆閲囨牱浠诲姟銆?
 * 杩欎竴灞傚彧璐熻矗瀹炰綋鎸夐敭銆佸€炬枩/璺屽€掍簨浠讹紝浠ュ強 CONNECT / MUSIC_ON 鐨勭姸鎬佽疆璇?
 * 鐢垫簮鍜屽厖鐢电姸鎬佹敼鐢?GPIO EXTI 涓柇鍙戦€侀槦鍒椼€?
 */
void KeyTask(void *argument)
{
    (void)argument;

    key_event_t key_event;
    TiltKey_t key;

    static uint32_t tilt_enable_timer = 0;

    while (1)
    {
        if (!g_ui.sys_running)
        {
            xSemaphoreTake(xKeyScanTaskWakeSemaphore, portMAX_DELAY);
        }

        {
            if (Key_Scan(&key_event))
            {
                xQueueSend(Key_Power_queue, &key_event, portMAX_DELAY);
                tilt_enable_timer = 300;
            }

            if (tilt_enable_timer > 0U)
            {
                key = TiltKey_Update(&g_sensors_motion);
                if (key != MSG_TILT_NONE)
                {
                    xQueueSend(Key_Music_queue, &key, portMAX_DELAY);
                    tilt_enable_timer = 300;
                    log_printf("Tilt Key Detected: %d\r\n", key);
                }
                tilt_enable_timer--;
            }

            key = FallDetect_Check(&g_sensors_motion);
            if (key == MSG_FALL_DOWN)
            {
                xQueueSend(Key_Music_queue, &key, portMAX_DELAY);
            }
        }

        vTaskDelay(10);
    }
}
