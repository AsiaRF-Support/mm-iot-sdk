/*
 *  Copyright (c) 2025 AsiaRF
 *  All rights reserved.
 */


#include "arf_relay.h"
#include "main.h"
#include "mmosal.h"

#define STABLE_PWM_LEVEL	PWM_LEVEL_50
#define RELAY_DELAY_MS		100


relay_config_t relay_config(TIM_HandleTypeDef *htim, uint32_t channel, GPIO_TypeDef *port, uint16_t pin, relay_type_t rtype)
{
    relay_config_t config = {
        .pwm_pin = pwm_config(htim, channel),
		.port = port,
		.pin = pin,
		.rtype = rtype
    };

	pwm_set_level(&config.pwm_pin, PWM_OFF);
	pwm_start(&config.pwm_pin);
    return config;
}


void relay_on(const relay_config_t *relay)
{
	if (relay->rtype != MONOSTABLE_RELAY && relay->rtype != BISTABLE_RELAY) {
		__ERR("Unknown relay type.");
		return;
	}

	if (relay->rtype == MONOSTABLE_RELAY) {
		pwm_set_level(&relay->pwm_pin, PWM_LEVEL_MAX);
		mmosal_task_sleep(RELAY_DELAY_MS);
		pwm_set_level(&relay->pwm_pin, STABLE_PWM_LEVEL);
		
		return;
	}
	else if (relay->rtype == BISTABLE_RELAY) {
		pwm_set_level(&relay->pwm_pin, PWM_LEVEL_MAX);
		mmosal_task_sleep(RELAY_DELAY_MS);
		pwm_set_level(&relay->pwm_pin, PWM_OFF);

		return;
	}
}


void relay_off(const relay_config_t *relay)
{
	if (relay->rtype != MONOSTABLE_RELAY && relay->rtype != BISTABLE_RELAY) {
		__ERR("Unknown relay type.");
		return;
	}

	if (relay->rtype == MONOSTABLE_RELAY) {
		pwm_set_level(&relay->pwm_pin, PWM_OFF);
		
		return;
	}
	else if (relay->rtype == BISTABLE_RELAY) {
		HAL_GPIO_WritePin(relay->port, relay->pin, GPIO_PIN_SET);
		mmosal_task_sleep(RELAY_DELAY_MS);
		HAL_GPIO_WritePin(relay->port, relay->pin, GPIO_PIN_RESET);

		return;
	}
}


/*
uint8_t relay_get_state(const relay_config_t *relay)
{
}
*/
