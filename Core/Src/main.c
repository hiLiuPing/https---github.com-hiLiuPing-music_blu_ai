/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "lptim.h"
#include "quadspi.h"
#include "rtc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "user_TasksInit.h"
#include "oled_ui.h"
#include "systemMonitor_app.h"
#include "data_app.h"
#include "led_app.h"
#include "log.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

static void Sleep_DisableStopWakeIrqs(void)
{
  HAL_NVIC_DisableIRQ(USART1_IRQn);
  HAL_NVIC_DisableIRQ(USART3_IRQn);
  HAL_NVIC_DisableIRQ(DMA1_Channel3_IRQn);
  HAL_NVIC_DisableIRQ(DMA1_Channel4_IRQn);
  HAL_NVIC_DisableIRQ(DMA1_Channel5_IRQn);
  HAL_NVIC_DisableIRQ(I2C2_EV_IRQn);
  HAL_NVIC_DisableIRQ(I2C2_ER_IRQn);
  // HAL_NVIC_DisableIRQ(TIM7_IRQn);

  NVIC_ClearPendingIRQ(USART1_IRQn);
  NVIC_ClearPendingIRQ(USART3_IRQn);
  NVIC_ClearPendingIRQ(DMA1_Channel3_IRQn);
  NVIC_ClearPendingIRQ(DMA1_Channel4_IRQn);
  NVIC_ClearPendingIRQ(DMA1_Channel5_IRQn);
  NVIC_ClearPendingIRQ(I2C2_EV_IRQn);
  NVIC_ClearPendingIRQ(I2C2_ER_IRQn);
  // NVIC_ClearPendingIRQ(TIM7_IRQn);
}

static void Sleep_EnableRunIrqs(void)
{
  __HAL_UART_CLEAR_IDLEFLAG(&huart1);
  NVIC_ClearPendingIRQ(USART1_IRQn);
  NVIC_ClearPendingIRQ(USART2_IRQn);
  NVIC_ClearPendingIRQ(DMA1_Channel3_IRQn);
  NVIC_ClearPendingIRQ(DMA1_Channel4_IRQn);
  NVIC_ClearPendingIRQ(DMA1_Channel5_IRQn);
  NVIC_ClearPendingIRQ(I2C2_EV_IRQn);
  NVIC_ClearPendingIRQ(I2C2_ER_IRQn);
  // NVIC_ClearPendingIRQ(TIM7_IRQn);
  NVIC_ClearPendingIRQ(LPTIM1_IRQn);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
  HAL_NVIC_EnableIRQ(I2C2_EV_IRQn);
  HAL_NVIC_EnableIRQ(I2C2_ER_IRQn);
  // HAL_NVIC_EnableIRQ(TIM7_IRQn);
}

static uint32_t Sleep_ProcessPendingLptimWake(void)
{
  if (__HAL_LPTIM_GET_FLAG(&hlptim1, LPTIM_FLAG_ARRM) != RESET)
  {
    __HAL_LPTIM_CLEAR_FLAG(&hlptim1, LPTIM_FLAG_ARRM);
    EXTI->PR2 = LPTIM_EXTI_LINE_LPTIM1;
    NVIC_ClearPendingIRQ(LPTIM1_IRQn);
    LPTIM_OnTick();
    return 1U;
  }

  return 0U;
}

static void WakeRuntimeTasks(uint8_t wake_display)
{
  g_ui.sys_running = 1U;

  if (xKeyScanTaskWakeSemaphore != NULL)
    xSemaphoreGive(xKeyScanTaskWakeSemaphore);
  if (xLedTaskWakeSemaphore != NULL)
    xSemaphoreGive(xLedTaskWakeSemaphore);
  if (xOLedShowTaskWakeSemaphore != NULL)
    xSemaphoreGive(xOLedShowTaskWakeSemaphore);
  if (xTransmitTaskWakeSemaphore != NULL)
    xSemaphoreGive(xTransmitTaskWakeSemaphore);
  if (xAppDataTaskWakeSemaphore != NULL)
    xSemaphoreGive(xAppDataTaskWakeSemaphore);

  if (wake_display)
  {
    (void)OLED_UI_PostStateEvent(UI_EVT_SHOW_ON, "WakeRuntime");
    if (LED_cmd_queue != NULL)
     {
        LED_EVT_t evt = LED_EVT_BREATH_GREEN;
      xQueueSend(LED_cmd_queue, &evt, 0);
     } 
  }
}

static void HandlePowerSignalExti(uint16_t gpio_pin)
{
  TiltKey_t key = MSG_TILT_NONE;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if (Key_Music_queue == NULL)
  {
    return;
  }

  if (gpio_pin == POWER_IN_5V_Pin)
  {
    GPIO_PinState state = HAL_GPIO_ReadPin(POWER_IN_5V_GPIO_Port, POWER_IN_5V_Pin);
    key = (state == GPIO_PIN_RESET) ? MSG_PWER_IN : MSG_PWER_OUT;
  }
  else if (gpio_pin == BAT_CHG_Pin)
  {
    GPIO_PinState state = HAL_GPIO_ReadPin(BAT_CHG_GPIO_Port, BAT_CHG_Pin);
    key = (state == GPIO_PIN_RESET) ? MSG_PWER_CHAGNE : MSG_PWER_FULL;
  }

  if (key == MSG_TILT_NONE)
  {
    return;
  }

  (void)xQueueSendFromISR(Key_Music_queue, &key, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void vApplicationMallocFailedHook(void)
{
  taskDISABLE_INTERRUPTS();
  for (;;);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  (void)xTask;
  (void)pcTaskName;
  taskDISABLE_INTERRUPTS();
  for (;;);
}

void vApplicationIdleHook(void)
{
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_QUADSPI_Init();
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  MX_LPTIM1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */
  User_Tasks_Init();
  vTaskStartScheduler();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 20;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
  * @brief  STOP wake clock recovery without HAL tick dependency.
  */
static void SystemClock_STOPResume(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    uint32_t timeout;

    MODIFY_REG(PWR->CR1, PWR_CR1_VOS, PWR_CR1_VOS_0);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 1;
    RCC_OscInitStruct.PLL.PLLN = 20;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_4);
    if (__HAL_FLASH_GET_LATENCY() != FLASH_LATENCY_4)
    {
        Error_Handler();
    }

    MODIFY_REG(RCC->CFGR, RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2,
               RCC_SYSCLK_DIV1 | RCC_HCLK_DIV1 | RCC_HCLK_DIV1);

    MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, RCC_SYSCLKSOURCE_PLLCLK);
    timeout = 200000;
    while (__HAL_RCC_GET_SYSCLK_SOURCE() != RCC_CFGR_SWS_PLL)
    {
        if (--timeout == 0)
        {
            Error_Handler();
        }
    }

    SystemCoreClock = HAL_RCC_GetSysClockFreq();
}
void EXTI2_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(SW_Pin);
    PWR->SCR = PWR_SCR_CWUF;
    (void)PWR->SCR;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == BAT_CHG_Pin || GPIO_Pin == POWER_IN_5V_Pin)
    {
        HandlePowerSignalExti(GPIO_Pin);
    }
}

void HAL_LPTIM_AutoReloadMatchCallback(LPTIM_HandleTypeDef *hlptim)
{
    if (hlptim->Instance == LPTIM1)
    {
        LPTIM_OnTick();
    }
}

void My_PreSleep_Function(uint32_t *x)
{
    if (x == NULL)
    {
        return;
    }

    if (g_ui.sys_running == 0)
    {
        HAL_SuspendTick();
        __HAL_GPIO_EXTI_CLEAR_IT(SW_Pin);
        __HAL_GPIO_EXTI_CLEAR_IT(BAT_CHG_Pin);
        __HAL_GPIO_EXTI_CLEAR_IT(POWER_IN_5V_Pin);
        HAL_NVIC_EnableIRQ(EXTI2_IRQn);
        __DSB();

        PWR->SCR = PWR_SCR_CWUF;
        (void)PWR->SCR;

        Sleep_DisableStopWakeIrqs();
        *x = 0U;
        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    }
}

void My_PostSleep_Function(uint32_t x)
{
    (void)x;
    if (g_ui.sys_running == 0)
    {
        uint32_t is_key_wake = (EXTI->PR1 & SW_Pin) != 0U;
        uint32_t is_power_wake = (EXTI->PR1 & (BAT_CHG_Pin | POWER_IN_5V_Pin)) != 0U;
        uint32_t is_lptim_wake = ((EXTI->PR2 & LPTIM_EXTI_LINE_LPTIM1) != 0U) ||
                                 (__HAL_LPTIM_GET_FLAG(&hlptim1, LPTIM_FLAG_ARRM) != RESET);
        SystemClock_STOPResume();

        HAL_NVIC_DisableIRQ(EXTI2_IRQn);
        NVIC_ClearPendingIRQ(EXTI2_IRQn);

        if (is_lptim_wake)
        {
            is_lptim_wake = Sleep_ProcessPendingLptimWake();
        }

        uint32_t has_lptim_event = (g_quote_ready || g_key_idle_timeout || g_bulu_timeout || g_music_timeout) ? 1U : 0U;

        HAL_ResumeTick();
        Sleep_EnableRunIrqs();

        if (is_key_wake || is_power_wake)
        {
            WakeRuntimeTasks(1U);
        }
        else if (is_lptim_wake)
        {
            if (has_lptim_event)
            {
                WakeRuntimeTasks(1U);
            }
        }
        else if (g_key_idle_timeout || g_bulu_timeout || g_music_timeout || g_quote_ready)
        {
            WakeRuntimeTasks(1U);
        }

        PWR->SCR = PWR_SCR_CWUF;
        (void)PWR->SCR;
    }
}
/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
