/*
 * Copyright 2025 AsiaRF
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mm_app_common.h"
#include "mmosal.h"


/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    printf("\n\nAsiaRF CFG demo (Built " __DATE__ " " __TIME__ ")\n\n");

#if defined (STM_USBD_ACM_ENABLE)
	extern void cfg_start(void);
	cfg_start();
#else
#error "STM_USBD_ACM_ENABLE didn't enable\r\n"
#endif

    app_wlan_init();

    app_wlan_start();

	while(1) {
		mmosal_task_sleep(100);
	}
}
