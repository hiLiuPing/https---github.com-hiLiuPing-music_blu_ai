/* Private includes -----------------------------------------------------------*/
// includes
#include "FreeRTOS.h"
#include "data_app.h"
#include "key.h"
#include "led_app.h"
#include "log.h"
#include "lptim.h"
#include "main.h"
#include "music_app.h"
#include "oled_ui.h"
#include "queue.h"
#include "systemMonitor_app.h"
#include "ui_nav.h"
#include "user_TasksInit.h"
#include "weather_app.h"

extern RGB_Object_t rgb;

extern volatile UI_Global_t g_ui;
static uint8_t s_shutdown_hold_active = 0U;
/* 宏：向 OLED 队列发送 UI 事件。 */
#define SEND_UI_EVT(evt) ((void)OLED_UI_PostEvent((evt), "KeyMgr"))

/*
 * 输入分发任务。
 * 这一层消费 KeyTask 投递的“原始输入消息”，再根据当前页面翻译成音乐、LED、UI 事件。
 */

void KeyManllegeTask(void *pvParameters)
{
    (void)pvParameters;

    key_event_t key_event;
    TiltKey_t key;
    LED_EVT_t led_t;
    QueueSetMemberHandle_t xActivatedMember; // 用于接收被激活的队列句柄
    while (1)
    {
        /* * 永远等待队列集中的任意一个队列有数据。
         * portMAX_DELAY 表示死等（永远等待），不消耗 CPU。
         */
        xActivatedMember = xQueueSelectFromSet(xKeyQueueSet, portMAX_DELAY);

        /* 判断是哪一个队列传来了数据 */
        if (xActivatedMember == Key_Power_queue)
        {
            // 从实体按键队列中读取数据（此时保证一定能读到，立刻读取，tick设为0）
            if (xQueueReceive(Key_Power_queue, &key_event, 0) == pdPASS)
            {
                Key_Event();

                if (key_event.type == KEY_EVT_CLICK)
                {
                    if (g_ui.oled_showing == 0)
                    {
                        (void)OLED_UI_PostStateEvent(UI_EVT_SHOW_ON, "KeyMgr");
                    }
                    // Weather_FillDemoData();
                    if (g_music_ble_state.music_ble_power == 0)
                    {
                        /* code */
                        music_send_cmd(CMD_POWER_ON);
                    }

                    // Music_PowerOn();
                    log_printf("Key %d Clicked!\n", key_event.id);
                }
                else if (key_event.type == KEY_EVT_LONG)
                {
                    if (g_ui.oled_showing == 0U)
                    {
                        (void)OLED_UI_PostStateEvent(UI_EVT_SHOW_ON, "KeyMgr");
                    }
                    music_send_cmd(CMD_PLAY_STOP);
                    SEND_UI_EVT(UI_EVT_PAUSE);
                    UI_Nav_ShutdownStart();
                    s_shutdown_hold_active = 1U;
                    log_printf("Key %d Long Pressed!\n", key_event.id);
                }
                else if (key_event.type == KEY_EVT_REPEAT)
                {
                    if (s_shutdown_hold_active)
                    {
                        UI_Nav_ShutdownKeepAlive();
                    }
                }
                else if (key_event.type == KEY_EVT_UP)
                {
                    if (s_shutdown_hold_active)
                    {
                        if ((Key_GetPressedMask() & KEY_BIT(key_event.id)) == 0U)
                        {
                            UI_Nav_ShutdownCancel();
                            s_shutdown_hold_active = 0U;
                        }
                    }
                }
            }
        }
        else if (xActivatedMember == Key_Music_queue)
        {
            // 从姿态/蓝牙队列中读取数据
            if (xQueueReceive(Key_Music_queue, &key, 0) == pdPASS)
            {
                Key_Event();

                switch (key)
                {
                case MSG_BLUETOOTH_CONNECT:
                    g_music_ble_state.ble_connected = 1;
                    g_music_ble_state.ble_ever_connected = 1;
                    LPTIM_Bulu_Connect();
                    SEND_UI_EVT(UI_EVT_BLUETOOTH_CONNECTED);
                    /*
                     * LED 与页面联动策略：
                     * 1. 蓝牙未连接时，红灯闪烁并回首页。
                     * 2. 蓝牙已连接但未播放时，绿灯闪烁并停留首页。
                     * 3. 正在播放时，绿灯呼吸并切到播放页。
                     */

                    if (g_music_ble_state.music_played == 0)
                    {
                        if (LED_cmd_queue != NULL)
                        {
                            led_t = LED_EVT_BLINK_GREEN;
                            xQueueSend(LED_cmd_queue, &led_t, portMAX_DELAY);
                        }
                    }
                    else
                    {

                        if (LED_cmd_queue != NULL)
                        {
                            led_t = LED_EVT_BREATH_GREEN;
                            xQueueSend(LED_cmd_queue, &led_t, portMAX_DELAY);
                        }
                    }
                    break;

                case MSG_BLUETOOTH_DISCONNECT:
                    g_music_ble_state.ble_connected = 0;
                    if (g_music_ble_state.ble_ever_connected)
                    {
                        LPTIM_Bulu_Disonnect(300U);
                    }
                    if (LED_cmd_queue != NULL)
                    {
                        led_t = LED_EVT_BLINK_RED;
                        xQueueSend(LED_cmd_queue, &led_t, portMAX_DELAY);
                    }

                    break;

                case MSG_MUSIC_PLAY:
                    if (g_music_ble_state.ble_connected == 0)
                    {
                        SEND_UI_EVT(UI_EVT_BLUETOOTH_DISCONNECTED);
                        break;
                    }
                    g_music_ble_state.music_played = 1;
                    g_music_ble_state.music_ever_played = 1;
                    LPTIM_Music_Play();
                    if (LED_cmd_queue != NULL)
                    {
                        led_t = LED_EVT_BREATH_GREEN;
                        xQueueSend(LED_cmd_queue, &led_t, portMAX_DELAY);
                    }

                    break;

                case MSG_MUSIC_STOP:
                    if (g_music_ble_state.ble_connected == 0)
                    {
                        SEND_UI_EVT(UI_EVT_BLUETOOTH_DISCONNECTED);
                        break;
                    }
                    g_music_ble_state.music_played = 0;
                    if (g_music_ble_state.music_ever_played)
                    {
                        LPTIM_Music_Stop(180U);
                    }
                    /*
                     * LED 与页面联动策略：
                     * 1. 蓝牙未连接时，红灯闪烁并回首页。
                     * 2. 蓝牙已连接但未播放时，绿灯闪烁并停留首页。
                     * 3. 正在播放时，绿灯呼吸并切到播放页。
                     */
                    if (g_music_ble_state.ble_connected == 0)
                    {
                        if (LED_cmd_queue != NULL)
                        {
                            led_t = LED_EVT_BLINK_RED;
                            xQueueSend(LED_cmd_queue, &led_t, portMAX_DELAY);
                        }
                        return;
                    }

                    if (LED_cmd_queue != NULL)
                    {
                        led_t = LED_EVT_BLINK_GREEN;
                        xQueueSend(LED_cmd_queue, &led_t, portMAX_DELAY);
                    }

                    break;

                case MSG_TILT_UP:
                    if (g_music_ble_state.ble_connected == 0)
                    {
                        SEND_UI_EVT(UI_EVT_BLUETOOTH_DISCONNECTED);
                        break;
                    }
                    music_send_cmd(CMD_VOL_UP);
                    if (LED_cmd_queue != NULL)
                    {
                        led_t = LED_EVT_HEARTBEAT_GREEN;
                        xQueueSend(LED_cmd_queue, &led_t, portMAX_DELAY);
                    }

                    SEND_UI_EVT(UI_EVT_VOL_UP);
                    log_printf("CMD_VOL_UP Button Pressed!\r\n");
                    break;

                case MSG_TILT_DOWN:
                    if (g_music_ble_state.ble_connected == 0)
                    {
                        SEND_UI_EVT(UI_EVT_BLUETOOTH_DISCONNECTED);
                        break;
                    }
                    music_send_cmd(CMD_VOL_DOWN);
                    if (LED_cmd_queue != NULL)
                    {
                        led_t = LED_EVT_HEARTBEAT_GREEN;
                        xQueueSend(LED_cmd_queue, &led_t, portMAX_DELAY);
                    }
                    SEND_UI_EVT(UI_EVT_VOL_DOWN);
                    log_printf("CMD_VOL_DOWN Button Pressed!\r\n");
                    break;

                case MSG_TILT_RIGHT:
                    if (g_music_ble_state.ble_connected == 0)
                    {
                        SEND_UI_EVT(UI_EVT_BLUETOOTH_DISCONNECTED);
                        break;
                    }
                    music_send_cmd(CMD_PREV);
                    if (LED_cmd_queue != NULL)
                    {
                        led_t = LED_EVT_HEARTBEAT_GREEN;
                        xQueueSend(LED_cmd_queue, &led_t, portMAX_DELAY);
                    }
                    SEND_UI_EVT(UI_EVT_PREV);
                    log_printf("CMD_PREV Button Pressed!\r\n");
                    break;

                case MSG_TILT_LEFT:
                    if (g_music_ble_state.ble_connected == 0)
                    {
                        SEND_UI_EVT(UI_EVT_BLUETOOTH_DISCONNECTED);
                        break;
                    }
                    music_send_cmd(CMD_NEXT);
                    if (LED_cmd_queue != NULL)
                    {
                        led_t = LED_EVT_HEARTBEAT_GREEN;
                        xQueueSend(LED_cmd_queue, &led_t, portMAX_DELAY);
                    }
                    SEND_UI_EVT(UI_EVT_NEXT);
                    break;

                case MSG_TILT_SHAKE_VERTICAL:
                    if (WeatherSyncTaskHandle != NULL)
                    {
                        xTaskNotifyGive(WeatherSyncTaskHandle);
                    }
                    SEND_UI_EVT(UI_EVT_WEATHER_TIME_SYNC);
                    log_printf("Tilt vertical shake: sync weather/time.\r\n");
                    break;

                case MSG_TILT_SHAKE_HORIZONTAL:

                    if (WeatherSyncTaskHandle != NULL)
                    {
                        xTaskNotifyGive(WeatherSyncTaskHandle);
                    }
                    SEND_UI_EVT(UI_EVT_WEATHER_TIME_SYNC);

                    log_printf("Tilt horizontal shake detected.\r\n");
                    break;

                case MSG_PWER_IN:
                    g_ui.battery.batterypower = 1;
                    g_ui.battery.is_charging =
                        (HAL_GPIO_ReadPin(BAT_CHG_GPIO_Port, BAT_CHG_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
                    SEND_UI_EVT(UI_EVT_POWERIN);
                    break;

                case MSG_PWER_OUT:
                    g_ui.battery.batterypower = 0;
                    g_ui.battery.is_charging = 0;
                    SEND_UI_EVT(UI_EVT_POWEROUT);
                    break;

                case MSG_PWER_CHAGNE:
                    g_ui.battery.is_charging = 1;
                    SEND_UI_EVT(UI_EVT_BATTERY_CHARGING);
                    break;

                case MSG_PWER_FULL:
                    g_ui.battery.is_charging = 0;
                    if (g_ui.battery.batterypower)
                    {
                        SEND_UI_EVT(UI_EVT_BATTERY_FULL);
                    }
                    break;

                case MSG_FALL_DOWN:
                    log_printf("Fall Down Detected!\r\n");
                    break;

                default:
                    break;
                }
            }
        }

        /* * 独立于队列之外的关机状态逻辑检查
         * 移到 switch 外层，每次有任意消息唤醒时顺便检查
         */
        if (s_shutdown_hold_active && (Key_GetPressedMask() & KEY_BIT(KEY_ID_B)) == 0U)
        {
            UI_Nav_ShutdownCancel();
            s_shutdown_hold_active = 0U;
        }
    }
}
