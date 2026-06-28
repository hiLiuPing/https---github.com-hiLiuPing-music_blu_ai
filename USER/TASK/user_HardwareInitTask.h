#ifndef __USER_HARDWAREINITTASK_H__
#define __USER_HARDWAREINITTASK_H__

/*
 * 上电初始化任务。
 * 负责硬件、Flash、LittleFS、UI 和基础应用模块的 bring-up。
 */

#ifdef __cplusplus
extern "C" {
#endif

/* 系统启动后执行一次，完成所有底层依赖的初始化。 */
void HardwareInitTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif
