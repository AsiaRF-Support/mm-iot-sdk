/*
 *	Copyright (c) 2025 AsiaRF
 *	All rights reserved.
 */

#ifndef __ARF_LED_H__
#define __ARF_LED_H__


#include "stm32u5xx.h"
#include "FreeRTOS.h"
#include "timers.h"

typedef enum {
    GPIO_ACTIVE_HIGH = 0,
    GPIO_ACTIVE_LOW,
} gpio_active_level_t;

typedef enum {
	LED_ERR = -1,
    LED_STATE_OFF,
    LED_STATE_ON
} led_state_t;

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t active_level;  // LED_GPIO_ACTIVE_HIGH 或 LED_GPIO_ACTIVE_LOW
} led_config_t;

typedef struct {
    led_config_t led_config;

    xTimerHandle xTimer_off_callback;
} led_ctrl_t;

led_config_t led_config(GPIO_TypeDef *port, uint16_t pin, gpio_active_level_t active_level);
void led_on(const led_config_t *led);
void led_off(const led_config_t *led);
void led_toggle(const led_config_t *led);
led_state_t led_state(const led_config_t *led);

#endif // __ARF_LED_H__
