/*
 *	Copyright (c) 2025 AsiaRF
 *	All rights reserved.
 */


#include "arf_led.h"
#include "main.h"


led_config_t led_config(GPIO_TypeDef *port, uint16_t pin, gpio_active_level_t active_level)
{
    led_config_t config = {
        .port = port,
        .pin = pin,
        .active_level = active_level
    };
    return config;
}

void led_on(const led_config_t *led)
{
	if (led->port == NULL) {
		__ERR("Unable to get led entry.\n");
		return;
	}

    if (led->active_level == GPIO_ACTIVE_HIGH) {
        HAL_GPIO_WritePin(led->port, led->pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(led->port, led->pin, GPIO_PIN_RESET);
    }
}

void led_off(const led_config_t *led)
{
	if (led->port == NULL) {
		__ERR("Unable to get led entry.\n");
		return;
	}

    if (led->active_level == GPIO_ACTIVE_HIGH) {
        HAL_GPIO_WritePin(led->port, led->pin, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(led->port, led->pin, GPIO_PIN_SET);
    }
}

void led_toggle(const led_config_t *led)
{
	if (led->port == NULL) {
		__ERR("Unable to get led entry.\n");
		return;
	}
	
	HAL_GPIO_TogglePin(led->port, led->pin);
}


led_state_t led_state(const led_config_t *led)
{
	if (led->port == NULL) {
		__ERR("Unable to get led entry.\n");
		return LED_ERR;
	}

    if (led->active_level == GPIO_ACTIVE_HIGH) {
        return HAL_GPIO_ReadPin(led->port, led->pin) == GPIO_PIN_SET ? LED_STATE_ON : LED_STATE_OFF;
    } else {
        return HAL_GPIO_ReadPin(led->port, led->pin) == GPIO_PIN_RESET ? LED_STATE_ON : LED_STATE_OFF;
    }
}

