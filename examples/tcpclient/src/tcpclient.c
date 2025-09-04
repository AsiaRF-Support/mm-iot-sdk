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

#define ECHO_PORT 7
#include "main.h"

#include <string.h>
#include "mmosal.h"
#include "mmwlan.h"
#include "mm_app_loadconfig.h"

#include "mm_app_common.h"

#define SERVER_IP "192.168.3.3"
#define SERVER_PORT 5000

static struct altcp_pcb *client_pcb;
static unsigned long count = 0;


static err_t tcp_client_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err)
{
    UNUSED(arg);

    if (err == ERR_OK && p != NULL) {
        printf("Received data: %.*s\n", p->len, (char *)p->payload);
        altcp_recved(pcb, p->len);
        pbuf_free(p);
    } else if (p == NULL) {
        printf("Connection closed by server\n");
        altcp_close(client_pcb);
    }

    return ERR_OK;
}


static err_t tcp_client_sent(void *arg, struct altcp_pcb *pcb, u16_t len)
{
    UNUSED(arg);
    UNUSED(pcb);
    UNUSED(len);

    printf("Data sent %lu successfully\n", count);

	altcp_recv(pcb, NULL);
	altcp_sent(pcb, NULL);

    altcp_close(pcb);

    return ERR_OK;
}


static err_t tcp_client_connected(void *arg, struct altcp_pcb *pcb, err_t err)
{
    UNUSED(arg);
	char message[128];

    if (err == ERR_OK) {
        printf("Connected to server\n");
        altcp_recv(pcb, tcp_client_recv);
        altcp_sent(pcb, tcp_client_sent);

		snprintf(message, sizeof(message), "Hello %lu from LWIP client", ++count);

        err_t send_err = altcp_write(pcb, message, strlen(message), TCP_WRITE_FLAG_COPY);
        if (send_err != ERR_OK) {
            printf("Failed to send data: %d\n", send_err);
        }
    } else {
        printf("Failed to connect to server: %d\n", err);
    }

    return err;
}


int tcp_client_connect(void)
{
    ip_addr_t server_ip;
    err_t err;

    if (!ipaddr_aton(SERVER_IP, &server_ip)) {
        printf("Invalid IP address\n");
        return -1;
    }

    client_pcb = altcp_new(NULL);
    if (client_pcb == NULL) {
        printf("Failed to create PCB\n");
        return -2;
    }

    err = altcp_connect(client_pcb, &server_ip, SERVER_PORT, tcp_client_connected);
	if (err != ERR_OK) {
		printf("altcp_connect failed: %d\n", err);
		return -3;
	}

	return 0;
}


/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    printf("\n\nAsiaRF TCP Client Demo (Built " __DATE__ " " __TIME__ ")\n\n");

    /* Initialize and connect to Wi-Fi, blocks till connected */
    app_wlan_init();

    app_wlan_start();

    mmosal_task_sleep(1000);

    printf("Start Application\n");

    while (count < 100) {

		LOCK_TCPIP_CORE();

    	tcp_client_connect();

		UNLOCK_TCPIP_CORE();

		mmosal_task_sleep(100);
    }

    /* Disconnect from Wi-Fi */
    app_wlan_stop();
}

