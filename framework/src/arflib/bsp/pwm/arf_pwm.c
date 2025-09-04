/*
 *  Copyright (c) 2025 AsiaRF
 *  All rights reserved.
 */


#include "arf_pwm.h"
#include "main.h"



static uint16_t get_pwm_ccr(uint32_t arr, uint8_t level);
static uint8_t get_pwm_level(uint32_t arr, uint16_t ccr);


//pwm_config_t pwm_config(TIM_HandleTypeDef *htim, uint32_t channel, uint32_t arr)
pwm_config_t pwm_config(TIM_HandleTypeDef *htim, uint32_t channel)
{
    pwm_config_t config = {
        .htim = htim,
        .channel = channel
        //.arr = arr
    };
    return config;
}

void pwm_start(const pwm_config_t *pwm)
{
	if (pwm->htim == NULL) {
		__ERR("Unable to get timer entry.\n");
		return;
	}

	HAL_TIM_PWM_Start(pwm->htim, pwm->channel);
}

void pwm_stop(const pwm_config_t *pwm)
{
	if (pwm->htim == NULL) {
		__ERR("Unable to get timer entry.\n");
		return;
	}
	
	HAL_TIM_PWM_Stop(pwm->htim, pwm->channel);
}

void pwm_set_level(const pwm_config_t *pwm, uint8_t level)
{
	if (pwm->htim == NULL) {
		__ERR("Unable to get timer entry.\n");
		return;
	}

	if (level > 100) {
		__ERR("Out off pwm setting level range(0 ~ 100).");
		return;
	}

	uint32_t arr = __HAL_TIM_GET_AUTORELOAD(pwm->htim);
	__HAL_TIM_SET_COMPARE(pwm->htim, pwm->channel, get_pwm_ccr(arr, level));
}

int pwm_get_level(const pwm_config_t *pwm)
{
	if (pwm->htim == NULL) {
		__ERR("Unable to get timer entry.\n");
		return PWM_LEVEL_ERR;
	}

	uint32_t arr = __HAL_TIM_GET_AUTORELOAD(pwm->htim);
	uint16_t crr = __HAL_TIM_GET_COMPARE(pwm->htim, pwm->channel);

	return get_pwm_level(arr, crr);
}

static uint16_t get_pwm_ccr(uint32_t arr, uint8_t level)
{
	if (level >= 100) {
		return arr;
	}

    return (uint16_t)(level) * (arr + 1) / 100;
}

static uint8_t get_pwm_level(uint32_t arr, uint16_t ccr)
{
    if (arr == 0) {
		return 0;
	}

	uint32_t tmp = ((uint32_t)ccr * 100 + (arr + 1) / 2) / (arr + 1); // add-half method for round.
    if(tmp > 100) {
		tmp = 100;
	}

    return (uint8_t)tmp;
}
