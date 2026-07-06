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
 * 实时健康数据全局变量结构体。
 *
 * 只保留一个全局变量 g_health_data。
 * MAX30102 模块测到的数据、卡路里、运动时间、预警标志都会写到这里。
 * 后续蓝牙/App/语音模块需要数据时，直接读取 g_health_data 即可。
 *
 * alert_code 是“哪里出问题”的预警播报码，不再做很多等级。
 * 语音模块不用再做按位与判断，直接根据这个数值选择播报内容即可：
 * 0：无异常，不播异常提醒
 * 1：心率过低，建议播“注意，心率过低，请检查身体状态”
 * 2：心率过高，建议播“注意，心率过高，请降低骑行强度”
 * 4：血氧偏低，建议播“注意，血氧偏低，请降低骑行强度并休息”
 * 5：心率过低 + 血氧偏低，建议播“注意，心率过低且血氧偏低，请检查身体状态并休息”
 * 6：心率过高 + 血氧偏低，建议播“注意，心率过高且血氧偏低，请停止骑行并休息”
 *
 * normal_report_flag 用于正常骑行时的周期播报：
 * 0：没有到正常播报时间
 * 1：已到正常播报时间，语音模块可以播报一次正常数据，播完后由语音模块清 0
 * 正常播这个：已骑行 X 分钟，消耗约 Y 千卡，当前心率 Z，血氧 W
 * normal_report_count 表示已经触发过几次正常播报，方便调试。
 */
typedef struct
{
  volatile int heart_rate;                /* 当前心率，单位 bpm */
  volatile int spo2;                      /* 当前血氧，单位 % */
  volatile int valid;                     /* MAX30102 数据有效标志，1 表示有效 */
  volatile uint32_t kcal_x100;            /* 累计卡路里*100，例如 18.36 kcal 存 1836 */
  volatile uint32_t sport_time_s;         /* 有效运动时间，单位秒 */
  volatile uint8_t alert_code;           /* 预警播报码，表示心率/血氧哪里异常 */
  volatile uint8_t normal_report_flag;    /* 正常数据播报标志，1 表示到 3 分钟播报点 */
  volatile uint32_t normal_report_count;  /* 正常数据播报触发次数 */
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
