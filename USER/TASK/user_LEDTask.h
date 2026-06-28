#ifndef __USER_LEDTASK_H__
#define __USER_LEDTASK_H__

/* LED/RGB 动画任务：周期刷新灯效状态机。 */

#ifdef __cplusplus
extern "C" {
#endif

/* 持续驱动 LED 和 RGB 动画。 */
void LEDTask(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif
