/*
 *  Copyright (c) 2025 AsiaRF
 *  All rights reserved.
 */


#include "arf_relay.h"
#include "main.h"
#include "mmosal.h"

// Suggest at least 70% of 3.3v for ORWH-SH-112D1F monostate relay.
#define STABLE_PWM_LEVEL	PWM_LEVEL_70
#define RELAY_DELAY_MS		100


relay_config_t relay_config(TIM_HandleTypeDef *htim, uint32_t channel, GPIO_TypeDef *port, uint16_t pin, relay_type_t rtype, gpio_active_level_t active_level)
{
    relay_config_t config = {
        .pwm_pin = pwm_config(htim, channel),
		.port = port,
		.pin = pin,
		.rtype = rtype,
		.active_level = active_level
    };

	if (active_level == GPIO_ACTIVE_HIGH) {
		pwm_set_level(&config.pwm_pin, PWM_OFF);
	}
	else if (active_level == GPIO_ACTIVE_LOW) {
		pwm_set_level(&config.pwm_pin, PWM_LEVEL_MAX);
	}

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

		// If active_level == GPIO_ACTIVE_HIGH
		uint8_t pwm_max_level = PWM_LEVEL_MAX;
		uint8_t pwm_stable_level = STABLE_PWM_LEVEL;

		if (relay->active_level == GPIO_ACTIVE_LOW) {
			pwm_max_level = PWM_OFF;
			pwm_stable_level = PWM_LEVEL_MAX - STABLE_PWM_LEVEL;
		}

		pwm_set_level(&relay->pwm_pin, pwm_max_level);
		mmosal_task_sleep(RELAY_DELAY_MS);
		pwm_set_level(&relay->pwm_pin, pwm_stable_level);
		
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
		if (relay->active_level == GPIO_ACTIVE_HIGH) {
			pwm_set_level(&relay->pwm_pin, PWM_OFF);
		}
		else if (relay->active_level == GPIO_ACTIVE_LOW) {
			pwm_set_level(&relay->pwm_pin, PWM_LEVEL_MAX);
		}
		
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
