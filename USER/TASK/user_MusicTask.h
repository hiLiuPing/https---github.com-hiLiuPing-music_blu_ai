#ifndef __USER_MUSICTASK_H__
#define __USER_MUSICTASK_H__

/* 音乐控制任务：把上层命令转换成蓝牙按键模拟和供电控制。 */

#ifdef __cplusplus
extern "C" {
#endif

/* 阻塞等待音乐命令队列，并驱动外部蓝牙音箱模块。 */
void MusicTask(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif
