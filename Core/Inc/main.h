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
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
/*
 * 手部节点实时健康数据。
 * 仍然只使用一个全局变量 g_health_data，但心率和血氧各自拥有独立的
 * 有效标志与预警码，便于触点选择“心率/血氧/卡路里”时分别判断。
 *
 * 心率预警码 hr_alert_code：
 * 0：心率正常，不播异常
 * 1：心率过低
 * 2：心率过高
 *
 * 血氧预警码 spo2_alert_code：
 * 0：血氧正常，不播异常
 * 3：血氧过低
 *
 * 因为心率和血氧是分别选择、分别播报，所以不再生成组合预警码。
 */

typedef struct
{
  volatile int heart_rate;             /* 当前心率，单位 bpm */
  volatile int spo2;                   /* 当前血氧，单位 % */
  volatile int hr_valid;               /* 心率有效标志：1有效，0无效 */
  volatile int spo2_valid;             /* 血氧有效标志：1有效，0无效 */
  volatile int valid;                  /* 兼容标志：任意一项有效时为1 */
  volatile uint32_t kcal_x100;         /* 累计卡路里*100 */
  volatile uint32_t sport_time_s;      /* 有效心率对应的运动时间，单位秒 */
  volatile uint8_t hr_alert_code;      /* 0正常，1心率过低，2心率过高 */
  volatile uint8_t spo2_alert_code;    /* 0正常，3血氧过低 */
} Health_Data_t;

typedef struct {
  int32_t heart_rate;
  int32_t spo2;
  uint32_t kcal;
  uint32_t sport_time;
  uint32_t alert_code;
} health_data_t;

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
#define RST_Pin GPIO_PIN_1
#define RST_GPIO_Port GPIOA
#define MAX30102_INT_Pin GPIO_PIN_5
#define MAX30102_INT_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
