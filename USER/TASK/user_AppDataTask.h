#ifndef __USER_APPDATATASK_H__
#define __USER_APPDATATASK_H__

/* 应用数据任务：刷新传感器、时间和语录弹窗数据。 */

#ifdef __cplusplus
extern "C" {
#endif

/* 以 30ms/1s 双节拍更新共享数据。 */
void AppDataTask(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif
