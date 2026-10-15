/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "stm32f4xx_hal.h"

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
#define KEY_V3_Pin GPIO_PIN_3
#define KEY_V3_GPIO_Port GPIOE
#define KEY_V4_Pin GPIO_PIN_4
#define KEY_V4_GPIO_Port GPIOE
#define USER_KEY_Pin GPIO_PIN_13
#define USER_KEY_GPIO_Port GPIOC
#define USER_KEY_EXTI_IRQn EXTI15_10_IRQn
#define PWM2_Pin GPIO_PIN_8
#define PWM2_GPIO_Port GPIOE
#define PWM1_Pin GPIO_PIN_9
#define PWM1_GPIO_Port GPIOE
#define PWM4_Pin GPIO_PIN_10
#define PWM4_GPIO_Port GPIOE
#define PWM3_Pin GPIO_PIN_11
#define PWM3_GPIO_Port GPIOE
#define KEY_H4_Pin GPIO_PIN_10
#define KEY_H4_GPIO_Port GPIOD
#define KEY_V1_Pin GPIO_PIN_11
#define KEY_V1_GPIO_Port GPIOD
#define FLASH_CS_Pin GPIO_PIN_15
#define FLASH_CS_GPIO_Port GPIOA
#define OLED_D0_Pin GPIO_PIN_10
#define OLED_D0_GPIO_Port GPIOC
#define OLED_DC_Pin GPIO_PIN_11
#define OLED_DC_GPIO_Port GPIOC
#define OLED_D1_Pin GPIO_PIN_12
#define OLED_D1_GPIO_Port GPIOC
#define OLED_RES_Pin GPIO_PIN_0
#define OLED_RES_GPIO_Port GPIOD
#define OLED_CS_Pin GPIO_PIN_1
#define OLED_CS_GPIO_Port GPIOD
#define KEY_H1_Pin GPIO_PIN_3
#define KEY_H1_GPIO_Port GPIOD
#define KEY_H2_Pin GPIO_PIN_4
#define KEY_H2_GPIO_Port GPIOD
#define KEY_H3_Pin GPIO_PIN_7
#define KEY_H3_GPIO_Port GPIOD
#define KEY_V2_Pin GPIO_PIN_1
#define KEY_V2_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
