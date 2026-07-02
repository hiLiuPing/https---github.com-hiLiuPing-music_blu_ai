#ifndef __USER_TASKSINIT_H__
#define __USER_TASKSINIT_H__

/*
 * FreeRTOS 启动入口。
 * 这里统一暴露任务句柄和消息队列，方便其它模块按“任务/队列”边界协作。
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
/* 输入与控制消息队列。 */
extern QueueHandle_t Key_Power_queue;
extern QueueHandle_t Key_Music_queue;
extern QueueHandle_t music_cmd_queue;
extern QueueHandle_t OLED_UI_queue;
extern QueueHandle_t LED_cmd_queue;





/* 任务句柄。 */
extern TaskHandle_t HardwareInitTaskHandle;
extern TaskHandle_t MusicTaskHandle;
extern TaskHandle_t KeyTaskHandle;
extern TaskHandle_t KeyManllegeTaskHandle;
extern TaskHandle_t LEDTaskHandle;
extern TaskHandle_t OLEDTaskHandle;
extern TaskHandle_t MusicFFTTaskHandle;
extern TaskHandle_t TransmitTaskHandle;
extern TaskHandle_t AppDataTaskHandle;
extern TaskHandle_t WeatherSyncTaskHandle;
extern QueueSetHandle_t xKeyQueueSet;


//按键扫描信号量
extern SemaphoreHandle_t xKeyScanTaskWakeSemaphore;
extern SemaphoreHandle_t xAppDataTaskWakeSemaphore;
extern SemaphoreHandle_t xLedTaskWakeSemaphore;
extern SemaphoreHandle_t xOLedShowTaskWakeSemaphore;
extern SemaphoreHandle_t xTransmitTaskWakeSemaphore;
extern SemaphoreHandle_t xWeatherSyncTaskWakeSemaphore;

/* 创建全部队列和任务，并作为调度器启动前的总装配入口。 */
void User_Tasks_Init(void);
void Key_QueueSet_Init(void);   
#ifdef __cplusplus
}
#endif

#endif
