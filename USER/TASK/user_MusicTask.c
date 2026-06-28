/* Private includes -----------------------------------------------------------*/
// includes
#include "user_TasksInit.h"
#include "user_MusicTask.h"
#include "main.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "music_app.h"
#include "log.h"

/*
 * 音乐控制执行任务。
 * 上层只往队列里塞抽象命令，这里再把命令落成实体按键动作。
 */

void MusicTask(void *pvParameters)
{
    (void)pvParameters;

    MusicCtrlCmd cmd;

    // 任务启动后先给蓝牙音箱上电，保证后续命令有执行环境。
    // Music_PowerOn();
    
    vTaskDelay(pdMS_TO_TICKS(50));

    while (1)
    {
        // 阻塞等待命令，避免空转占用 CPU。
        if (xQueueReceive(music_cmd_queue, &cmd, portMAX_DELAY) == pdPASS)
        {
            switch (cmd)
            {
            case CMD_PLAY_STOP:
                Music_Play_Stop();
                break;
            case CMD_PREV:
                Music_Up();
                break;
            case CMD_NEXT:
                Music_Next();
                break;
            case CMD_PAIR:
                Music_Pair();
                break;
            case CMD_CLEAR_PAIR:
                Music_ClearPair();
                break;
            case CMD_POWER_ON:
                Music_PowerOn();
                break;
            case CMD_POWER_OFF:
                Music_PowerOff();
                break;
            case CMD_VOL_UP:
                Music_VolumeUp();
                break;
            case CMD_VOL_DOWN:
                Music_VolumeDown();
                break;
            default:
                break;
            }
        }
    }
}
