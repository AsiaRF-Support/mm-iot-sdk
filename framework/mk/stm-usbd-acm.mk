#
# Copyright (c) 2024-2025 AsiaRF
#
# Command line interface for control HaLow parameters
# SPDX-License-Identifier: Apache-2.0
#
#

include $(MMIOT_ROOT)/mk/stm-usbd-core.mk

STM_USBD_CDC_DIR = src/stm-usbd/Middlewares/ST/STM32_USB_Device_Library/Class/CDC

STM_USBD_CDC_SRCS_C += Src/usbd_cdc.c

STM_USBD_CDC_SRCS_H += Inc/usbd_cdc.h



MMIOT_SRCS_C += $(addprefix $(STM_USBD_CDC_DIR)/,$(STM_USBD_CDC_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(STM_USBD_CDC_DIR)/,$(STM_USBD_CDC_SRCS_H))


MMIOT_INCLUDES += $(STM_USBD_CDC_DIR)/Inc

CFLAGS += -DSTM_USBD_ACM_ENABLE
CFLAGS-$(STM_USBD_CDC_DIR) += -Wno-c++-compat

