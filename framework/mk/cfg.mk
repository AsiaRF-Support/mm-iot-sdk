#
# Copyright (c) 2024-2025 AsiaRF
#
# SPDX-License-Identifier: Apache-2.0
#


CLI_CORE_DIR = src/stm-usbd/Middlewares/Third_Party/FreeRTOS-Plus/FreeRTOS-Plus-CLI


CLI_CORE_SRCS_C += FreeRTOS_CLI.c

CLI_CORE_SRCS_H += FreeRTOS_CLI.h


MMIOT_SRCS_C += $(addprefix $(CLI_CORE_DIR)/,$(CLI_CORE_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(CLI_CORE_DIR)/,$(CLI_CORE_SRCS_H))


MMIOT_INCLUDES += $(CLI_CORE_DIR)

CFLAGS-$(CLI_CORE_DIR) += -Wno-c++-compat
CFLAGS += -DCFG_ENABLE
