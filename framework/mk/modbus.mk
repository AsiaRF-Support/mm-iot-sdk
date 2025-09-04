#
# Copyright (c) 2024-2025 AsiaRF
#
# SPDX-License-Identifier: Apache-2.0
#


MODBUS_DIR = src/modbus


MODBUS_SRCS_C += Modbus.c
MODBUS_SRCS_C += UARTCallback.c


MODBUS_SRCS_H += Modbus.h
MODBUS_SRCS_H += ModbusConfig.h



MMIOT_SRCS_C += $(addprefix $(MODBUS_DIR)/,$(MODBUS_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(MODBUS_DIR)/,$(MODBUS_SRCS_H))



MMIOT_INCLUDES += $(MODBUS_DIR)

CFLAGS += -DENABLE_MODBUS=1

# Ignore UART unused init code
#CFLAGS-$(BSP_DIR) += -Wno-unused-function
#CFLAGS-$(BSP_DIR) += -Wno-error

CFLAGS-$(MODBUS_DIR) += -Wno-c++-compat

