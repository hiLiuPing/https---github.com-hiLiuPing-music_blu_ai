#ifndef __USER_OLEDTASK_H__
#define __USER_OLEDTASK_H__

/* OLED 主渲染任务：固定帧率推进 UI 状态机并提交画面。 */

#ifdef __cplusplus
extern "C" {
#endif

/* 约 60FPS 的 UI 主循环。 */
void OLEDTask(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif
