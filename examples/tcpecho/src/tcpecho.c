/*
 * Copyright 2024-2025 AsiaRF
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lwip/altcp.h"
#include "lwip/altcp_tcp.h"
#include "lwip/tcp.h"
#include "lwip/tcpip.h"
#include "lwip/init.h"
#include "lwip/ip_addr.h"
#include "lwip/mem.h"
#include <stdio.h>

#include "main.h"

#include <string.h>
#include "mmosal.h"
#include "mmwlan.h"

#include "mm_app_common.h"

#define ECHO_PORT 7

/*
*/
static err_t echo_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err)
{
	LWIP_UNUSED_ARG(arg);
	LWIP_UNUSED_ARG(err);

    if (p == NULL) {
		printf("echo_recv close\r\n");
        altcp_close(pcb);
        return ERR_OK;
    }
	else {
		printf("echo_recv entry\r\n");
	}

	altcp_recved(pcb, p->tot_len);

    altcp_write(pcb, p->payload, p->len, TCP_WRITE_FLAG_COPY);

    altcp_output(pcb);

    pbuf_free(p);

    return ERR_OK;
}


/*
*/
static void echo_err(void *arg, err_t err)
{
	LWIP_UNUSED_ARG(arg);
	printf("Connection Error: %d\r\n", err);
}


/*
*/
static err_t echo_accept(void *arg, struct altcp_pcb *new_pcb, err_t err)
{
	LWIP_UNUSED_ARG(arg);
	LWIP_UNUSED_ARG(err);

    altcp_recv(new_pcb, echo_recv);
 	altcp_err(new_pcb, echo_err);
	printf("Accept\r\n");
	return ERR_OK;
}

/*
*/
void echo_init(void) {
    struct altcp_pcb *pcb;

    pcb = altcp_tcp_new();

    if (pcb == NULL) {
        printf("Error creating PCB\n");
        return;
    }

	if (altcp_bind(pcb, IP_ANY_TYPE, ECHO_PORT) != ERR_OK) {
        printf("Error binding PCB\n");
        altcp_close(pcb);
        return;
    }

    pcb = altcp_listen(pcb);
    if (pcb == NULL) {
        printf("Error listening\n");
        return;
    }

    altcp_accept(pcb, echo_accept);

    printf("TCP Echo server is running on port %d\n", ECHO_PORT);
}



/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    printf("\n\nAsiaRF TCP Echo Server Demo (Built " __DATE__ " " __TIME__ ")\n\n");

    /* Initialize and connect to Wi-Fi, blocks till connected */
    app_wlan_init();
    app_wlan_start();

    mmosal_task_sleep(1000);


	LOCK_TCPIP_CORE();

	echo_init();

	UNLOCK_TCPIP_CORE();

	while(1) {
		mmosal_task_sleep(100);
	}

    /* Disconnect from Wi-Fi */
    app_wlan_stop();
}
