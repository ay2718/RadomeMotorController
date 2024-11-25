/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
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
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "constants.h"
#include "stdint.h"
#include "stdbool.h"

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

#define AZIMUTH
//#define ELEVATION

#ifdef AZIMUTH
#ifdef MOTOR_DEFINED
#error Multiple motor definition!
#endif
#define MOTOR_DEFINED

#define ID_STR "AZIMUTH"

#define OFFSET 0x90000000
#define ID 1

//#define GEAR_REDUCTION 488.7f
#define VEL_FUDGE_FACTOR 1.04f
#define GEAR_REDUCTION 472.0f
//#define CAN_ID 0x10

#define VEL_KP 30.0f
#define VEL_HPF 10.0f
#define VEL_KD 0.3f
#define VEL_POS_GAIN 2.0f

#define PDC_MAX_CURRENT 10.0f


#endif

#ifdef ELEVATION
#ifdef MOTOR_DEFINED
#error Multiple motor definition!
#endif
#define MOTOR_DEFINED

#define ID_STR "ELEVATION"

#define OFFSET 0xa2000000
#define ID 2

#define VEL_FUDGE_FACTOR 1.04f
#define GEAR_REDUCTION 1078.21f
//#define CAN_ID 0x20

#define VEL_KP 30.0f
#define VEL_HPF 10.0f
#define VEL_KD 0.3f
#define VEL_POS_GAIN 1.0f

#define PDC_MAX_CURRENT 15.0f

#define EL_UPPER_HARDSTOP uint32_t(int32_t(0x1p32f * 2.0f / 360.0f)) // note that elevation is REVERSED!!!
#define EL_LOWER_HARDSTOP uint32_t(int32_t(0x1p32f * -87.0f / 360.0f))

#endif

#ifndef MOTOR_DEFINED
#error No motor defined!
#endif

#define MAX_MOTOR_SLEW 40.0f
#define MAX_MOTOR_VELOCITY 200.0f
//#define MAX_MOTOR_VELOCITY 50.0f
#define VELOCITY_LSB ((3.0f * PI) / (3600.0f * 180.0f)) // 3 arcseconds/s, max speed is about 27 dps, earth rotation is 5 LSB
#define CURRENT_LSB 0.001f // LSB is 1 mA
#define VBUS_LSB 0.5f

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ADC_SAMPLES 4
#define CLOCK_FREQ 170000000
#define DEAD_TIME 140
#define ADC_SEQ_LEN 2
#define PWM_ADC_TRIG (ADC_CLK_SCALE * ADC_SEQ_LEN * ADC_SAMPLES / 2)
#define PWM_FREQ 20000
#define ADC_CLK_SCALE (3 * 25)
#define PWM_PERIOD (CLOCK_FREQ / (PWM_FREQ * 2))
#define ADC_SW_OFFSET 180
#define SOC_Pin GPIO_PIN_0
#define SOC_GPIO_Port GPIOA
#define SOB_Pin GPIO_PIN_1
#define SOB_GPIO_Port GPIOA
#define SOA_Pin GPIO_PIN_2
#define SOA_GPIO_Port GPIOA
#define VBUS_SENSE_Pin GPIO_PIN_3
#define VBUS_SENSE_GPIO_Port GPIOA
#define SPI1_CS_Pin GPIO_PIN_4
#define SPI1_CS_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
