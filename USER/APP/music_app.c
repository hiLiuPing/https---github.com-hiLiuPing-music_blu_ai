#include "music_app.h"

#include <stdio.h>

#include "log.h"
#include "music_fft_app.h"
Music_Ble_State_t g_music_ble_state = {0};

/*
 * 音乐控制抽象层。
 * 这一层把“抽象命令”翻译成实体按键 GPIO 脉冲，方便替换真正的蓝牙音箱模块。
 */

extern QueueHandle_t music_cmd_queue;

const KeyGpioTypeDef key_gpio[KEY_NUM] = {

    {DOWN_GPIO_Port, DOWN_Pin},
    {UP_GPIO_Port, UP_Pin},
    {PLAY_GPIO_Port, PLAY_Pin},

};

/**
 * @brief  向音乐任务发送控制命令
 * @param  cmd: 抽象控制命令
 * @retval None
 */
void music_send_cmd(MusicCtrlCmd cmd)
{
    MusicCtrlCmd stale_cmd;

    if (music_cmd_queue != NULL)
    {
        if (xQueueSend(music_cmd_queue, &cmd, 0) == pdPASS)
        {
            return;
        }

        (void)xQueueReceive(music_cmd_queue, &stale_cmd, 0);
        if (xQueueSend(music_cmd_queue, &cmd, 0) != pdPASS)
        {
            log_printf("[MusicCmdQ] drop cmd=%d\r\n", (int)cmd);
        }
    }
}

/**
 * @brief  模拟一次短按
 * @param  key: 目标按键
 * @retval None
 */
void SimKey_Click(KeyIndexTypeDef key)
{
    if (key >= KEY_NUM)
    {
        return;
    }

    HAL_GPIO_WritePin(key_gpio[key].port, key_gpio[key].pin, GPIO_PIN_SET);
    vTaskDelay(pdMS_TO_TICKS(100));
    HAL_GPIO_WritePin(key_gpio[key].port, key_gpio[key].pin, GPIO_PIN_RESET);
    vTaskDelay(pdMS_TO_TICKS(50));
}

/**
 * @brief  模拟一次双击
 * @param  key: 目标按键
 * @retval None
 */
void SimKey_DoubleClick(KeyIndexTypeDef key)
{
    SimKey_Click(key);
    vTaskDelay(pdMS_TO_TICKS(100));
    SimKey_Click(key);
}

/**
 * @brief  模拟长按
 * @param  key: 目标按键
 * @param  sec: 按下时长，单位秒
 * @retval None
 */
void SimKey_LongPress(KeyIndexTypeDef key, uint32_t sec)
{
    if (key >= KEY_NUM)
    {
        return;
    }

    // uint32_t ms = (uint32_t)(sec * 1000.0f);

    HAL_GPIO_WritePin(key_gpio[key].port, key_gpio[key].pin, GPIO_PIN_SET);
    vTaskDelay(pdMS_TO_TICKS(sec));
    HAL_GPIO_WritePin(key_gpio[key].port, key_gpio[key].pin, GPIO_PIN_RESET);
    vTaskDelay(pdMS_TO_TICKS(100));
}

/**
 * @brief  上一曲
 */
void Music_Up(void)
{
    log_printf("Music_Up\r\n");
    SimKey_Click(KEY2);
}

/**
 * @brief  播放/暂停切换
 */
void Music_Play_Stop(void)
{
    log_printf("Music_Play_Stop\r\n");
    SimKey_Click(KEY3);
}

/**
 * @brief  下一曲
 */
void Music_Next(void)
{
    log_printf("Music_Next\r\n");
    SimKey_Click(KEY1);
}

/**
 * @brief  进入蓝牙配对模式
 */
void Music_Pair(void)
{
    log_printf("Music_Pair\r\n");
    SimKey_LongPress(KEY3, 3000);
}

/**
 * @brief  清除蓝牙配对记录
 */
void Music_ClearPair(void)
{
    log_printf("Music_ClearPair\r\n");
    SimKey_LongPress(KEY3, 5000);
}

/**
 * @brief  打开蓝牙音箱模块电源
 */
void Music_PowerOn(void)
{
    log_printf("Music_PowerOn\r\n");

    HAL_GPIO_WritePin(AD_PWER_EN_GPIO_Port, AD_PWER_EN_Pin, GPIO_PIN_SET);
    vTaskDelay(pdMS_TO_TICKS(50));
    HAL_GPIO_WritePin(BULU_PWR_EN_GPIO_Port, BULU_PWR_EN_Pin, GPIO_PIN_SET);
    g_music_ble_state.music_ble_power = 1;
    if (MusicFFT_Start() != HAL_OK)
    {
        log_printf("[MusicFFT] start failed.\r\n");
    }
}

/**
 * @brief  关闭蓝牙音箱模块电源
 */
void Music_PowerOff(void)
{
    log_printf("Music_PowerOff\r\n");
    MusicFFT_Stop();
    HAL_GPIO_WritePin(BULU_PWR_EN_GPIO_Port, BULU_PWR_EN_Pin, GPIO_PIN_RESET);
    vTaskDelay(pdMS_TO_TICKS(50));
    HAL_GPIO_WritePin(AD_PWER_EN_GPIO_Port, AD_PWER_EN_Pin, GPIO_PIN_RESET);
    g_music_ble_state.music_ble_power = 0;
}

/**
 * @brief  音量加
 */
void Music_VolumeUp(void)
{
    log_printf("Music_VolumeUp\r\n");
    SimKey_LongPress(KEY1, 2000);
}

/**
 * @brief  音量减
 */
void Music_VolumeDown(void)
{
    log_printf("Music_VolumeDown\r\n");
    SimKey_LongPress(KEY2, 2000);
}

/**
 * @brief  系统断电
 */
void System_PowerOff(void)
{
    log_printf("System_PowerOff\r\n");
    HAL_GPIO_WritePin(ARM_RST_GPIO_Port, ARM_RST_Pin, GPIO_PIN_SET);
}
