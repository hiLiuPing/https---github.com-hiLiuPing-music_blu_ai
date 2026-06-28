#ifndef __USER_KEYTASK_H__
#define __USER_KEYTASK_H__

/* 原始输入采样任务：负责实体按键、倾斜消息和电源/充电状态采样。 */

#ifdef __cplusplus
extern "C" {
#endif

/* 将 GPIO 与姿态输入转换为统一消息，投递到业务队列。 */
void KeyTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif
