#ifndef __USER_TRANSMITTASK_H__
#define __USER_TRANSMITTASK_H__

/* 串口协议接收任务：从 DMA 环形缓冲中解析上位机/外部模块数据帧。 */

#ifdef __cplusplus
extern "C" {
#endif

/* 等待串口中断通知并解析完整协议帧。 */
void TransmitTask(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif
