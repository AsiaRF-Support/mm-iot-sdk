#
# Copyright (c) 2025 AsiaRF
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#


LIB_DIR = src/arflib


LIB_SRCS_C += bsp/led/arf_led.c
LIB_SRCS_C += bsp/pwm/arf_pwm.c
LIB_SRCS_C += component/relay/arf_relay.c
#LIB_SRCS_C += component/sensor/sht3x.c

LIB_SRCS_H += bsp/arf_platform.h
LIB_SRCS_H += bsp/led/arf_led.h
LIB_SRCS_H += bsp/pwm/arf_pwm.h
LIB_SRCS_H += component/relay/arf_relay.h
#LIB_SRCS_H += component/sensor/sht3x.h


MMIOT_SRCS_C += $(addprefix $(LIB_DIR)/,$(LIB_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(LIB_DIR)/,$(LIB_SRCS_H))

MMIOT_INCLUDES += $(LIB_DIR)/bsp
MMIOT_INCLUDES += $(LIB_DIR)/bsp/led
MMIOT_INCLUDES += $(LIB_DIR)/bsp/pwm
MMIOT_INCLUDES += $(LIB_DIR)/component/relay
MMIOT_INCLUDES += $(LIB_DIR)/component/sensor

CFLAGS-$(LIB_DIR) += -Wno-c++-compat

