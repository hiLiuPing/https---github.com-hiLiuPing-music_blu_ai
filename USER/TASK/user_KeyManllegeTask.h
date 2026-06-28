#ifndef __USER_KEYMANLLEGETASK_H__
#define __USER_KEYMANLLEGETASK_H__

/* 输入分发任务：把消息转换为音乐控制、LED 效果和 UI 事件。 */

#ifdef __cplusplus
extern "C" {
#endif

/* 消费 KeyTask 投递的消息，并按当前页面状态进行业务分发。 */
void KeyManllegeTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif
