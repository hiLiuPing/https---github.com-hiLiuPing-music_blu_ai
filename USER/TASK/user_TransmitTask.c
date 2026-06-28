/* Private includes -----------------------------------------------------------*/
// includes
#include "user_TasksInit.h"
#include "user_TransmitTask.h"
#include "main.h"
#include "oled_ui.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "weather_app.h"
#include "log.h"
#include "systemMonitor_app.h"

/*
 * 串口协议接收任务。
 * DMA 中断只负责唤醒，这里完成完整协议帧解析、CRC 校验和业务分发。
 */

void TransmitTask(void *arg)
{
    (void)arg;

    static ProtocolState_t state = STATE_IDLE;
    static uint8_t curr_cmd = 0;
    static uint16_t data_len = 0;
    static uint16_t data_idx = 0;
    static uint8_t payload[256];
    static uint8_t check_buf[260];
    static uint16_t check_ptr = 0;

    uint8_t ch;

        //    if (xTransmitTaskWakeSemaphore != NULL)
        // {
        //     xSemaphoreGive(xTransmitTaskWakeSemaphore);
        // }


    for (;;)
    {

// === 低功耗检查点 ===
        if (!g_ui.sys_running)
        {
            // log_printf("TransmitTask Enter Sleep...\r\n");

            // 1. 无限期等待信号量，直到系统整体被唤醒
            xSemaphoreTake(xTransmitTaskWakeSemaphore, portMAX_DELAY);

            // log_printf("TransmitTask Woken Up!\r\n");

            // 2. 关键核心：彻底清除休眠前或休眠期间可能产生的“残留任务通知”
            // 防止刚醒来就误触发一次无意义的协议解析
            xTaskNotifyStateClear(NULL); 
            (void)ulTaskNotifyTake(pdTRUE, 0); // 顺便把通知值清零

            // 3. 强行复位协议状态机，丢弃休眠前的半包/错包，防止醒来后数据错位粘包
            state = STATE_IDLE;
            check_ptr = 0;
            data_idx = 0;
        }


        // 等待 DMA 空闲中断通知，表示有一批新数据可解析。
        // 使用超时 (100ms) 而非无限等待，使任务能定期回检 sys_running，
        // 修复休眠时阻塞在此处无法进入低功耗检查点的 Bug。
        // if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) == 0)
        // {
        //     continue;  // 超时 → 回到 for(;;) 顶部检查 sys_running
        // }
          ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        // 按字节跑状态机，保证粘包和多帧也能逐帧恢复。
        while (uart_dma_read(&uart1_admin, &ch, 1, 0) > 0)
        {
            switch (state)
            {
            case STATE_IDLE:
                if (ch == FRAME_HEAD)
                {
                    state = STATE_CMD;
                    check_ptr = 0;
                }
                break;

            case STATE_CMD:
                curr_cmd = ch;
                check_buf[check_ptr++] = ch;
                state = STATE_LEN_H;
                break;

            case STATE_LEN_H:
                data_len = (uint16_t)ch << 8;
                check_buf[check_ptr++] = ch;
                state = STATE_LEN_L;
                break;

            case STATE_LEN_L:
                data_len |= ch;
                check_buf[check_ptr++] = ch;
                if (data_len > 250)
                {
                    state = STATE_IDLE;
                }
                else if (data_len == 0)
                {
                    state = STATE_CRC;
                }
                else
                {
                    data_idx = 0;
                    state = STATE_DATA;
                }
                break;

            case STATE_DATA:
                payload[data_idx++] = ch;
                check_buf[check_ptr++] = ch;
                if (data_idx >= data_len)
                {
                    state = STATE_CRC;
                }
                break;

            case STATE_CRC:
                // CRC 覆盖 CMD + LEN_H + LEN_L + DATA，不包含帧头尾。
                if (ch == stm32_calc_crc8(check_buf, check_ptr))
                {
                    state = STATE_TAIL;
                }
                else
                {
                    log_printf("[Protocol] CRC Error!\r\n");
                    state = STATE_IDLE;
                }
                break;

            case STATE_TAIL:
                if (ch == FRAME_TAIL)
                {
                    payload[data_len] = '\0';
                    process_protocol_data(curr_cmd, (char *)payload);
                }
                else
                {
                    log_printf("[Protocol] Frame Tail Error: %02X\r\n", ch);
                }

                // 无论成功失败都复位到空闲态，等待下一帧。
                state = STATE_IDLE;
                check_ptr = 0;
                data_idx = 0;
                break;
            }
        }
    }
}
