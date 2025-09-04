#
# Copyright (c) 2024-2025 AsiaRF
#
# Command line interface for control HaLow parameters
# SPDX-License-Identifier: Apache-2.0
#
#


STM_USBD_DIR = src/stm-usbd/Middlewares/ST/STM32_USB_Device_Library/Core

STM_USBD_SRCS_C += Src/usbd_conf.c
STM_USBD_SRCS_C += Src/usbd_core.c
STM_USBD_SRCS_C += Src/usbd_ctlreq.c
STM_USBD_SRCS_C += Src/usbd_desc.c
STM_USBD_SRCS_C += Src/usb_device.c
STM_USBD_SRCS_C += Src/usbd_ioreq.c


STM_USBD_SRCS_H += Inc/usbd_conf.h
STM_USBD_SRCS_H += Inc/usbd_core.h
STM_USBD_SRCS_H += Inc/usbd_ctlreq.h
STM_USBD_SRCS_H += Inc/usbd_def.h
STM_USBD_SRCS_H += Inc/usbd_desc.h
STM_USBD_SRCS_H += Inc/usb_device.h
STM_USBD_SRCS_H += Inc/usbd_ioreq.h



MMIOT_SRCS_C += $(addprefix $(STM_USBD_DIR)/,$(STM_USBD_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(STM_USBD_DIR)/,$(STM_USBD_SRCS_H))


MMIOT_INCLUDES += $(STM_USBD_DIR)/Inc

CFLAGS += -DSTM_USBD_ENABLE
CFLAGS-$(STM_USBD_DIR) += -Wno-c++-compat

