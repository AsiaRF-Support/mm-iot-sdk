/*
 *  Copyright (c) 2025 AsiaRF
 *  All rights reserved.
 */

#ifndef __ARF_RELAY_H__
#define __ARF_RELAY_H__

#include "stm32u5xx.h"
#include "FreeRTOS.h"
#include "timers.h"
#include "arf_pwm.h"

typedef enum {
	MONOSTABLE_RELAY = 0,
	BISTABLE_RELAY,
} relay_type_t;

typedef struct {
	pwm_config_t pwm_pin;
	GPIO_TypeDef *port;
	uint16_t pin;
	relay_type_t rtype;
} relay_config_t;

typedef struct
{
    relay_config_t relay_config;

    xTimerHandle xTimer_off_callback;
} relay_ctrl_t;

relay_config_t relay_config(TIM_HandleTypeDef *htim, uint32_t channel, GPIO_TypeDef *port, uint16_t pin, relay_type_t rtype);
void relay_on(const relay_config_t *relay);
void relay_off(const relay_config_t *relay);
//uint8_t relay_get_state(const relay_config_t *relay);


#endif // __ARF_RELAY_H__
