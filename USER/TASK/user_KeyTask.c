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

static void KeyTask_LogQueueDrop(const char *queue_name, uint32_t *drop_counter)
{
    (*drop_counter)++;
    if (((*drop_counter) & 0x07UL) == 0UL)
    {
        log_printf("[%s] dropped=%lu\r\n", queue_name, (unsigned long)(*drop_counter));
    }
}

static void KeyTask_SendPowerEvent(const key_event_t *evt)
{
    static uint32_t s_power_drop_count = 0U;

    if (Key_Power_queue == NULL || evt == NULL)
    {
        return;
    }

    if (xQueueSend(Key_Power_queue, evt, 0) != pdPASS)
    {
        KeyTask_LogQueueDrop("KeyPowerQ", &s_power_drop_count);
    }
}

static void KeyTask_SendMusicEvent(TiltKey_t key)
{
    static uint32_t s_music_drop_count = 0U;

    if (Key_Music_queue == NULL)
    {
        return;
    }

    if (xQueueSend(Key_Music_queue, &key, 0) != pdPASS)
    {
        KeyTask_LogQueueDrop("KeyMusicQ", &s_music_drop_count);
    }
}

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
                KeyTask_SendPowerEvent(&key_event);
                tilt_enable_timer = 300;
            }

            if (tilt_enable_timer > 0U)
            {
                key = TiltKey_Update(&g_sensors_motion);
                if (key != MSG_TILT_NONE)
                {
                    KeyTask_SendMusicEvent(key);
                    tilt_enable_timer = 300;
                    log_printf("Tilt Key Detected: %d\r\n", key);
                }
                tilt_enable_timer--;
            }

            key = FallDetect_Check(&g_sensors_motion);
            if (key == MSG_FALL_DOWN)
            {
                KeyTask_SendMusicEvent(key);
            }
        }

        vTaskDelay(10);
    }
}
