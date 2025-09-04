#
# Copyright (c) 2024-2025 AsiaRF
#
# SPDX-License-Identifier: Apache-2.0
#


CMSIS_RTOS2_DIR = src/RTOS2/FreeRTOS



CMSIS_RTOS2_SRCS_C += Source/ARM/clib_arm.c
CMSIS_RTOS2_SRCS_C += Source/cmsis_os1.c
CMSIS_RTOS2_SRCS_C += Source/cmsis_os2.c
CMSIS_RTOS2_SRCS_C += Source/freertos_evr.c
#CMSIS_RTOS2_SRCS_C += Source/handlers.c
CMSIS_RTOS2_SRCS_C += Source/os_systick.c




CMSIS_RTOS2_SRCS_H += Include/freertos_evr.h
CMSIS_RTOS2_SRCS_H += Include/freertos_mpool.h
CMSIS_RTOS2_SRCS_H += Include/freertos_os2.h
CMSIS_RTOS2_SRCS_H += Include1/cmsis_os.h
CMSIS_RTOS2_SRCS_H += Include1/cmsis_os2.h
CMSIS_RTOS2_SRCS_H += Include1/os_tick.h




MMIOT_SRCS_C += $(addprefix $(CMSIS_RTOS2_DIR)/,$(CMSIS_RTOS2_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(CMSIS_RTOS2_DIR)/,$(CMSIS_RTOS2_SRCS_H))



MMIOT_INCLUDES += $(CMSIS_RTOS2_DIR)/Include
MMIOT_INCLUDES += $(CMSIS_RTOS2_DIR)/Include1


CFLAGS-$(CMSIS_RTOS2_DIR) += -Wno-c++-compat
CFLAGS-$(ARV_LIB_DIR) += -Wno-unused-but-set-variable
CFLAGS-$(ARV_LIB_DIR) += -Wno-unused-parameter
CFLAGS-$(ARV_LIB_DIR) += -Wno-unused-variable

CFLAGS += -DINCLUDE_xTaskGetSchedulerState=1
CFLAGS += -DINCLUDE_uxTaskGetStackHighWaterMark=1
CFLAGS += -DINCLUDE_xTimerPendFunctionCall=1

