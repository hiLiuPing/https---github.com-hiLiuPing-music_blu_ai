/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define CONNECT_Pin GPIO_PIN_0
#define CONNECT_GPIO_Port GPIOC
#define DOWN_Pin GPIO_PIN_1
#define DOWN_GPIO_Port GPIOC
#define UP_Pin GPIO_PIN_2
#define UP_GPIO_Port GPIOC
#define PLAY_Pin GPIO_PIN_3
#define PLAY_GPIO_Port GPIOC
#define SW_Pin GPIO_PIN_0
#define SW_GPIO_Port GPIOA
#define LED_B_Pin GPIO_PIN_1
#define LED_B_GPIO_Port GPIOA
#define BAT_CHG_Pin GPIO_PIN_4
#define BAT_CHG_GPIO_Port GPIOA
#define LED_R_Pin GPIO_PIN_5
#define LED_R_GPIO_Port GPIOA
#define POWER_IN_5V_Pin GPIO_PIN_2
#define POWER_IN_5V_GPIO_Port GPIOB
#define LED_G_Pin GPIO_PIN_10
#define LED_G_GPIO_Port GPIOB
#define AD_PWER_EN_Pin GPIO_PIN_12
#define AD_PWER_EN_GPIO_Port GPIOB
#define ESP32_PWR_EN_Pin GPIO_PIN_6
#define ESP32_PWR_EN_GPIO_Port GPIOC
#define ARM_RST_Pin GPIO_PIN_7
#define ARM_RST_GPIO_Port GPIOC
#define BULU_PWR_EN_Pin GPIO_PIN_9
#define BULU_PWR_EN_GPIO_Port GPIOC
#define SPI1_CS_Pin GPIO_PIN_2
#define SPI1_CS_GPIO_Port GPIOD
#define MUSIC_ON_Pin GPIO_PIN_8
#define MUSIC_ON_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
