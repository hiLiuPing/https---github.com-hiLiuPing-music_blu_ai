/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lptim.c
  * @brief   This file provides code for the configuration
  *          of the LPTIM instances.
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
#include "lptim.h"

/* USER CODE BEGIN 0 */
#include <string.h>
#include "user_TasksInit.h"
/* USER CODE END 0 */

LPTIM_HandleTypeDef hlptim1;

/* LPTIM1 init function */
void MX_LPTIM1_Init(void)
{

  /* USER CODE BEGIN LPTIM1_Init 0 */

  /* USER CODE END LPTIM1_Init 0 */

  /* USER CODE BEGIN LPTIM1_Init 1 */

  /* USER CODE END LPTIM1_Init 1 */
  hlptim1.Instance = LPTIM1;
  hlptim1.Init.Clock.Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
  hlptim1.Init.Clock.Prescaler = LPTIM_PRESCALER_DIV128;
  hlptim1.Init.Trigger.Source = LPTIM_TRIGSOURCE_SOFTWARE;
  hlptim1.Init.OutputPolarity = LPTIM_OUTPUTPOLARITY_HIGH;
  hlptim1.Init.UpdateMode = LPTIM_UPDATE_IMMEDIATE;
  hlptim1.Init.CounterSource = LPTIM_COUNTERSOURCE_INTERNAL;
  hlptim1.Init.Input1Source = LPTIM_INPUT1SOURCE_GPIO;
  hlptim1.Init.Input2Source = LPTIM_INPUT2SOURCE_GPIO;
  if (HAL_LPTIM_Init(&hlptim1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPTIM1_Init 2 */

  /* USER CODE END LPTIM1_Init 2 */

}

void HAL_LPTIM_MspInit(LPTIM_HandleTypeDef* lptimHandle)
{

  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  if(lptimHandle->Instance==LPTIM1)
  {
  /* USER CODE BEGIN LPTIM1_MspInit 0 */

  /* USER CODE END LPTIM1_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LPTIM1;
    PeriphClkInit.Lptim1ClockSelection = RCC_LPTIM1CLKSOURCE_PCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    /* LPTIM1 clock enable */
    __HAL_RCC_LPTIM1_CLK_ENABLE();

    /* LPTIM1 interrupt Init */
    HAL_NVIC_SetPriority(LPTIM1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(LPTIM1_IRQn);
  /* USER CODE BEGIN LPTIM1_MspInit 1 */

  /* USER CODE END LPTIM1_MspInit 1 */
  }
}

void HAL_LPTIM_MspDeInit(LPTIM_HandleTypeDef* lptimHandle)
{

  if(lptimHandle->Instance==LPTIM1)
  {
  /* USER CODE BEGIN LPTIM1_MspDeInit 0 */

  /* USER CODE END LPTIM1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_LPTIM1_CLK_DISABLE();

    /* LPTIM1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(LPTIM1_IRQn);
  /* USER CODE BEGIN LPTIM1_MspDeInit 1 */

  /* USER CODE END LPTIM1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
/* LSE 32768 Hz / DIV128 = 256 Hz */
#define LPTIM_TICK_HZ           256U
#define LPTIM_ARR_1S            255U

#define KEY_IDLE_TIMEOUT_S      10800U

volatile uint8_t g_key_idle_timeout = 0;
volatile uint8_t g_io1_timeout = 0;
volatile uint8_t g_io2_timeout = 0;
volatile uint8_t g_quote_ready = 0;

typedef struct {
    uint32_t sec;
    uint32_t timeout;
    uint8_t active;
} SoftTimer_t;

typedef struct {
    SoftTimer_t quote;
    SoftTimer_t key_idle;
    SoftTimer_t io1;
    SoftTimer_t io2;
    uint32_t quote_interval;
} LPTIM1_SoftTimer_t;

static LPTIM1_SoftTimer_t s_tmr = {0};

void LPTIM_Start1Hz(void)
{
    EXTI->PR2 = LPTIM_EXTI_LINE_LPTIM1;
    NVIC_ClearPendingIRQ(LPTIM1_IRQn);
    memset(&s_tmr, 0, sizeof(s_tmr));
    HAL_LPTIM_Counter_Start_IT(&hlptim1, LPTIM_ARR_1S);
}

void LPTIM_Stop1Hz(void)
{
    HAL_LPTIM_Counter_Stop_IT(&hlptim1);
}

void LPTIM_ResetKeyIdle(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    s_tmr.key_idle.sec = 0;
    g_key_idle_timeout = 0;
    if (primask == 0U)
    {
        __enable_irq();
    }
}

void LPTIM_StartIO1(uint32_t timeout_s)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    s_tmr.io1.active = 1;
    s_tmr.io1.sec = 0;
    s_tmr.io1.timeout = timeout_s;
    g_io1_timeout = 0;
    if (primask == 0U)
    {
        __enable_irq();
    }
}

void LPTIM_StopIO1(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    s_tmr.io1.active = 0;
    s_tmr.io1.sec = 0;
    g_io1_timeout = 0;
    if (primask == 0U)
    {
        __enable_irq();
    }
}

void LPTIM_StartIO2(uint32_t timeout_s)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    s_tmr.io2.active = 1;
    s_tmr.io2.sec = 0;
    s_tmr.io2.timeout = timeout_s;
    g_io2_timeout = 0;
    if (primask == 0U)
    {
        __enable_irq();
    }
}

void LPTIM_StopIO2(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    s_tmr.io2.active = 0;
    s_tmr.io2.sec = 0;
    g_io2_timeout = 0;
    if (primask == 0U)
    {
        __enable_irq();
    }
}

void LPTIM_SetQuoteInterval(uint32_t sec)
{
    s_tmr.quote_interval = sec;
    s_tmr.quote.sec = 0;
}

void LPTIM_OnTick(void)
{
    s_tmr.key_idle.sec++;
    if (s_tmr.key_idle.sec >= KEY_IDLE_TIMEOUT_S)
    {
        s_tmr.key_idle.sec = 0;
        g_key_idle_timeout = 1;
    }

    if (s_tmr.io1.active)
    {
        s_tmr.io1.sec++;
        if (s_tmr.io1.sec >= s_tmr.io1.timeout)
        {
            s_tmr.io1.active = 0;
            g_io1_timeout = 1;
        }
    }

    if (s_tmr.io2.active)
    {
        s_tmr.io2.sec++;
        if (s_tmr.io2.sec >= s_tmr.io2.timeout)
        {
            s_tmr.io2.active = 0;
            g_io2_timeout = 1;
        }
    }

    s_tmr.quote.sec++;
    if (s_tmr.quote.sec >= s_tmr.quote_interval)
    {
        s_tmr.quote.sec = 0;
        g_quote_ready = 1;
        BaseType_t xTaskWoken = pdFALSE;
        if (AppDataTaskHandle != NULL)
        {
            vTaskNotifyGiveFromISR(AppDataTaskHandle, &xTaskWoken);
        }
        portYIELD_FROM_ISR(xTaskWoken);
    }
}

/* USER CODE END 1 */
