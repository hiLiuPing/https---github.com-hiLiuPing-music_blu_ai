/* Private includes -----------------------------------------------------------*/
// includes
#include "user_TasksInit.h"
//sys
// #include "sys.h"
#include "stdio.h"

//bsp
#include "key.h"
#include "music_app.h"
#include "data_app.h"
#include "oled_ui.h"
#include "systemMonitor_app.h"
//gui
// #include "lvgl.h"
// #include "lv_lib_pm.h"

//tasks
#include "user_HardwareInitTask.h"
// #include "user_LVGLTask.h"
// #include "user_PDUFPTask.h"
#include "user_KeyTask.h"
#include "user_MusicTask.h"
// #include "user_MessageTask.h"
#include "user_LEDTask.h"
#include "user_OLEDTask.h"
#include "user_TransmitTask.h"
#include "user_AppDataTask.h"
#include "user_KeyManllegeTask.h"
#include "user_WeatherSyncTask.h"

/* 私有变量 ---------------------------------------------------------*/

// 所有任务句柄统一在这里集中创建，方便后续暂停、恢复和调试。
TaskHandle_t HardwareInitTaskHandle;
TaskHandle_t MusicTaskHandle;
TaskHandle_t KeyTaskHandle;
TaskHandle_t KeyManllegeTaskHandle;
TaskHandle_t LEDTaskHandle;
TaskHandle_t OLEDTaskHandle;
TaskHandle_t TransmitTaskHandle;
TaskHandle_t AppDataTaskHandle;
TaskHandle_t WeatherSyncTaskHandle = NULL;

// 输入与音乐控制消息队列。
QueueHandle_t Key_Power_Queue = NULL;
QueueHandle_t Key_Music_queue = NULL;
QueueHandle_t music_cmd_queue = NULL;
QueueHandle_t OLED_UI_queue = NULL;

// 1. 定义信号量句柄（全局变量，方便其他任务或中断函数 extern 引用）
SemaphoreHandle_t xKeyScanTaskWakeSemaphore = NULL;
// 1. 定义信号量句柄（全局变量，方便其他任务或中断函数 extern 引用）
SemaphoreHandle_t xAppDataTaskWakeSemaphore = NULL;
// 1. 定义信号量句柄（全局变量，方便其他任务或中断函数 extern 引用）
SemaphoreHandle_t xLedTaskWakeSemaphore = NULL;

// 1. 定义信号量句柄（全局变量，方便其他任务或中断函数 extern 引用）
SemaphoreHandle_t xOLedShowTaskWakeSemaphore = NULL;

// 1. 定义信号量句柄（全局变量，方便其他任务或中断函数 extern 引用）
SemaphoreHandle_t xTransmitTaskWakeSemaphore = NULL;

SemaphoreHandle_t xWeatherSyncTaskWakeSemaphore = NULL;




// 1. 定义队列集句柄
QueueSetHandle_t xKeyQueueSet;

void Key_QueueSet_Init(void)
{
    // 2. 创建队列集，大小为两个队列容量的总和
    // 假设两个队列的长度都是 5，这里填 10
    xKeyQueueSet = xQueueCreateSet(10); 
    
    // 3. 将现有的两个队列添加进集合中
    xQueueAddToSet(Key_Power_Queue, xKeyQueueSet);
    xQueueAddToSet(Key_Music_queue, xKeyQueueSet);
}

/**
 * @brief  FreeRTOS 初始化入口
 * @note   这里统一完成队列和任务创建，调度器接管后进入长期运行。
 *         任务优先级采用“硬件初始化最高，其余业务任务并行”的方式。
 * @retval None
 */
void User_Tasks_Init(void)
{
    /* add mutexes, ... */
    /* add semaphores, ... */
    /* add queues, ... */
   // 2. 创建二值信号量
    xKeyScanTaskWakeSemaphore = xSemaphoreCreateBinary();
    xAppDataTaskWakeSemaphore = xSemaphoreCreateBinary();
    xLedTaskWakeSemaphore = xSemaphoreCreateBinary();
    xOLedShowTaskWakeSemaphore = xSemaphoreCreateBinary();
    xTransmitTaskWakeSemaphore = xSemaphoreCreateBinary();
    xWeatherSyncTaskWakeSemaphore = xSemaphoreCreateBinary();

    Key_Power_Queue = xQueueCreate(5, sizeof(key_event_t));
    Key_Music_queue = xQueueCreate(5, sizeof(TiltKey_t));
    music_cmd_queue = xQueueCreate(5, sizeof(MusicCtrlCmd));
    OLED_UI_queue = xQueueCreate(10, sizeof(UI_Event_t));
    Key_QueueSet_Init();

    /* add threads, ... */
    xTaskCreate(HardwareInitTask, "HwInitTask", 128 * 10, NULL, tskIDLE_PRIORITY + 5, &HardwareInitTaskHandle);
    xTaskCreate(KeyTask, "KeyTask", 128, NULL, tskIDLE_PRIORITY + 2, &KeyTaskHandle);
    xTaskCreate(MusicTask, "MusicTask", 128, NULL, tskIDLE_PRIORITY + 2, &MusicTaskHandle);
    xTaskCreate(LEDTask, "LEDTask", 128, NULL, tskIDLE_PRIORITY + 2, &LEDTaskHandle);
    xTaskCreate(OLEDTask, "OLEDTask", 320, NULL, tskIDLE_PRIORITY + 2, &OLEDTaskHandle);
    xTaskCreate(TransmitTask, "TransmitTask", 256, NULL, tskIDLE_PRIORITY + 2, &TransmitTaskHandle);
    xTaskCreate(AppDataTask, "AppDataTask", 512, NULL, tskIDLE_PRIORITY + 2, &AppDataTaskHandle);
    xTaskCreate(WeatherSyncTask, "WeatherSync", 192, NULL, tskIDLE_PRIORITY + 1, &WeatherSyncTaskHandle);
    xTaskCreate(KeyManllegeTask, "KeyManllegeTask", 160, NULL, tskIDLE_PRIORITY + 2, &KeyManllegeTaskHandle);
}
