/*
 *  Copyright (c) 2025 AsiaRF
 *  All rights reserved.
 */

#ifndef __ARF_PWM_H__
#define __ARF_PWM_H__

#include "stm32u5xx.h"
#include "FreeRTOS.h"
#include "timers.h"

typedef enum {
	PWM_LEVEL_ERR = -1,
	PWM_OFF = 0,
	PWM_LEVEL_10 = 10,
	PWM_LEVEL_20 = 20,
	PWM_LEVEL_30 = 30,
	PWM_LEVEL_40 = 40,
	PWM_LEVEL_50 = 50,
	PWM_LEVEL_60 = 60,
	PWM_LEVEL_70 = 70,
	PWM_LEVEL_80 = 80,
	PWM_LEVEL_90 = 90,
	PWM_LEVEL_MAX = 100,
} pwm_level_t;

typedef struct {
	TIM_HandleTypeDef *htim;
	uint32_t channel;
	//uint32_t arr; // AutoReloadPreload
} pwm_config_t;

typedef struct
{
    pwm_config_t pwm_config;

    xTimerHandle xTimer_off_callback;
} pwm_ctrl_t;

//pwm_config_t pwm_config(TIM_HandleTypeDef *htim, uint32_t channel, uint32_t arr);
pwm_config_t pwm_config(TIM_HandleTypeDef *htim, uint32_t channel);
void pwm_start(const pwm_config_t *pwm);
void pwm_stop(const pwm_config_t *pwm);
void pwm_set_level(const pwm_config_t *pwm, uint8_t level);
int pwm_get_level(const pwm_config_t *pwm);


#endif // __ARF_PWM_H__
