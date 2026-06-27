#include "freertos_app.h"

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "log.h"
#include "usart.h"
static void LedBlinkTask(void *argument);
static void FaultBlinkLoop(volatile uint32_t delay_cycles);

#define LED_BLINK_TASK_STACK_SIZE 128U

void MX_FREERTOS_Init(void)
{
  BaseType_t status;

  status = xTaskCreate(LedBlinkTask,
                       "LedBlink",
                       LED_BLINK_TASK_STACK_SIZE,
                       NULL,
                       tskIDLE_PRIORITY + 1U,
                       NULL);
  configASSERT(status == pdPASS);
}

void vApplicationMallocFailedHook(void)
{
  taskDISABLE_INTERRUPTS();
  // HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
  for (;;)
  {
  }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  (void)xTask;
  (void)pcTaskName;

  taskDISABLE_INTERRUPTS();
  // FaultBlinkLoop(200000U);
}

static void LedBlinkTask(void *argument)
{
  (void)argument;
log_init(&huart2);
  for (;;)
  {
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    log_printf("LedBlinkTask");
    vTaskDelay(pdMS_TO_TICKS(500));
    log_printf("aaaa");
  }
}

static void FaultBlinkLoop(volatile uint32_t delay_cycles)
{
  for (;;)
  {
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    for (volatile uint32_t i = 0; i < delay_cycles; i++)
    {
    }
  }
}
