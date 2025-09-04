/*
 * Copyright 2022-2023 Morse Micro
 *
 * This file is licensed under terms that can be found in the LICENSE.md file in the root
 * directory of the Morse Micro IoT SDK software package.
 */

/**
 * @file
 * @brief Template example application. This just enables the BSP and prints a message.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 */

#include <string.h>
#include "mmosal.h"

#include "mmwlan.h"
#include "mm_app_common.h"

#include "cmsis_os.h"

#include "Modbus.h"
#include "CLI-commands.h"

#include <lwip/api.h>
#include <lwip/tcp.h>
#include <lwip/tcpip.h>


#define RS485_BUF_SIZE			128
#define RS485_BASE_PORT			502
#define RS485_BASE_ID			1
//#define RS485_MASTER_TIMEOUT	30000
//#define RS485_SLAVE_TIMEOUT	60000
#define RS485_MASTER_TIMEOUT	pdMS_TO_TICKS(5000)
#define RS485_SLAVE_TIMEOUT		pdMS_TO_TICKS(1500)
#define RS485_DISABLE_EN		NULL


#ifndef NELEMS
#define NELEMS(p_)				(int)(sizeof(p_) / sizeof((p_)[0]))
#endif

extern UART_HandleTypeDef *pta_UART[MODBUS_NUM];

uint16_t u16aa_ModbusDATA[MODBUS_NUM][RS485_BUF_SIZE];
modbus_t taa_Telegram[MODBUS_NUM][2];

modbusHandler_t ta_Modbus_S[MODBUS_NUM];
modbusHandler_t ta_Modbus_M[MODBUS_NUM];


#ifdef ENABLE_DATA_LED
led_ctrl_t tx_led_ctrl[MODBUS_NUM];
led_ctrl_t rx_led_ctrl[MODBUS_NUM];

void Init_tx_led_config()
{
#if defined AWH575_MF1
	//tx_led_ctrl[0].led_config = led_config(RS485_TX_GPIO_Port, RS485_TX_Pin, GPIO_ACTIVE_HIGH);
#elif defined AWH575_MF1_V2
    tx_led_ctrl[0].led_config = led_config(RS485_LED_GPIO_Port, RS485_LED_Pin, GPIO_ACTIVE_LOW);
    tx_led_ctrl[1].led_config = led_config(RS232_LED_GPIO_Port, RS232_LED_Pin, GPIO_ACTIVE_LOW);
#elif defined AWMHU5_001
	tx_led_ctrl[0].led_config = led_config(LED_UR_TX_GPIO_Port, LED_UR_TX_Pin, GPIO_ACTIVE_LOW);
#elif defined AWH575_MF4
	tx_led_ctrl[0].led_config = led_config(LED_TX1_GPIO_Port, LED_TX1_Pin, GPIO_ACTIVE_HIGH);
	tx_led_ctrl[1].led_config = led_config(LED_TX2_GPIO_Port, LED_TX2_Pin, GPIO_ACTIVE_HIGH);
	tx_led_ctrl[2].led_config = led_config(LED_TX3_GPIO_Port, LED_TX3_Pin, GPIO_ACTIVE_HIGH);
	tx_led_ctrl[3].led_config = led_config(LED_TX4_GPIO_Port, LED_TX4_Pin, GPIO_ACTIVE_HIGH);
#else
	#error "TX LED does not setup"
#endif
}

void Init_rx_led_config()
{
#if defined AWH575_MF1
	//rx_led_ctrl[0].led_config = led_config(RS485_RX_GPIO_Port, RS485_RX_Pin, GPIO_ACTIVE_HIGH);
#elif defined AWH575_MF1_V2
    rx_led_ctrl[0].led_config = led_config(RS485_LED_GPIO_Port, RS485_LED_Pin, GPIO_ACTIVE_LOW);
    rx_led_ctrl[1].led_config = led_config(RS232_LED_GPIO_Port, RS232_LED_Pin, GPIO_ACTIVE_LOW);
#elif defined AWMHU5_001
	rx_led_ctrl[0].led_config = led_config(LED_UR_RX_GPIO_Port, LED_UR_RX_Pin, GPIO_ACTIVE_LOW);
#elif defined AWH575_MF4
	rx_led_ctrl[0].led_config = led_config(LED_RX1_GPIO_Port, LED_RX1_Pin, GPIO_ACTIVE_HIGH);
	rx_led_ctrl[1].led_config = led_config(LED_RX2_GPIO_Port, LED_RX2_Pin, GPIO_ACTIVE_HIGH);
	rx_led_ctrl[2].led_config = led_config(LED_RX3_GPIO_Port, LED_RX3_Pin, GPIO_ACTIVE_HIGH);
	rx_led_ctrl[3].led_config = led_config(LED_RX4_GPIO_Port, LED_RX4_Pin, GPIO_ACTIVE_HIGH);
#else
	#error "RX LED does not setup"
#endif
}

void vTimerCallback_led_off(TimerHandle_t xTimer) {
    led_ctrl_t *led_ctrl = (led_ctrl_t *)pvTimerGetTimerID(xTimer);
    if (led_ctrl) {
		led_off(&led_ctrl->led_config);
	}
	else {
		printf("Erorr: LED GPIO Port is NULL\n");
	}
}

#endif


/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
	printf("\r\n\r\n");
	printf("\n\nAsiaRF Modbus Demo "RELEASE_VERSION" (Built: " RELEASE_BUILD_DATE ")\n\n");

#if defined (STM_USBD_ACM_ENABLE)
	void cfg_start(void);
	cfg_start();
#else
	__ERR("STM_USBD_ACM_ENABLE didn't enable", ENDL);
#endif

    // HERE: Application code
#ifdef ENABLE_DATA_LED
		Init_tx_led_config();
		Init_rx_led_config();
#endif

	for (int i = 0; i < MODBUS_NUM; i ++) {
		char szName[16];
		char dev[4];
		char buf[BUFFER_SIZE] = {0};
		
		sprintf(szName, "master_%d", i+1);
		Modbus_Master_Configure(&ta_Modbus_M[i], u16aa_ModbusDATA[i], NELEMS(u16aa_ModbusDATA[i]), pta_UART[i], RS485_MASTER_TIMEOUT, RS485_DISABLE_EN, FLOW_CTR_RS485_MODE, szName);
		sprintf(szName, "slave_%d", i+1);
		snprintf(dev, sizeof(dev), "%u", i+1);
		if(config_read_str_of_dev(CLI_SET_MODBUS_PORT, buf, sizeof(buf), dev) > 0 && isValidModbusPort(buf, strlen(buf)))
			Modbus_Slave_Configure(&ta_Modbus_S[i], u16aa_ModbusDATA[i], NELEMS(u16aa_ModbusDATA[i]), RS485_BASE_ID+i, RS485_SLAVE_TIMEOUT, atoi(buf), szName);
		else
			Modbus_Slave_Configure(&ta_Modbus_S[i], u16aa_ModbusDATA[i], NELEMS(u16aa_ModbusDATA[i]), RS485_BASE_ID+i, RS485_SLAVE_TIMEOUT, RS485_BASE_PORT+i, szName);

		ta_Modbus_M[i].session = i;
		ta_Modbus_M[i].pta_Telegram = (modbus_t *)taa_Telegram[i];
		ta_Modbus_M[i].matcher = &ta_Modbus_S[i];
		ta_Modbus_S[i].session = i;
		ta_Modbus_S[i].pta_Telegram = (modbus_t *)taa_Telegram[i];
		ta_Modbus_S[i].matcher = &ta_Modbus_M[i];

#ifdef ENABLE_DATA_LED
		Modbus_Led_Configure_tx(&ta_Modbus_M[i], &tx_led_ctrl[i], &vTimerCallback_led_off);
		Modbus_Led_Configure_rx(&ta_Modbus_M[i], &rx_led_ctrl[i], &vTimerCallback_led_off);
#endif

		ModbusInit(&ta_Modbus_M[i]);	// for 485
		ModbusInit(&ta_Modbus_S[i]);	// for TCP

	}

	/* Initialize and connect to WiFi, blocks till connected */
    app_wlan_init();

    app_wlan_start();

	for (int i = 0; i < MODBUS_NUM; i ++) {
		/* Start prepare modbus server */
		mmosal_semb_give(ta_Modbus_S[i].slave_ready_semb);
		mmosal_sem_give(ta_Modbus_M[i].master_ready_sem);
		mmosal_sem_give(ta_Modbus_M[i].master_ready_sem);

		ModbusStart(&ta_Modbus_M[i]);
		ModbusStart(&ta_Modbus_S[i]);
		
#ifdef PLATFORM_NAME
		printf("%s RS485 port %d Modbus TCP (%d) ready\n", PLATFORM_NAME, i+1, ta_Modbus_S[i].uTcpPort);
#else
		// Not defined. Display as generic.
		printf("Generic RS485 port %d Modbus TCP (%d) ready\n", i+1, ta_Modbus_S[i].uTcpPort);
#endif
		printf("\tBaudrate: \t%lu\n", ta_Modbus_M[i].port->Init.BaudRate);
		printf("\tData bit: \t%s\n", ta_Modbus_M[i].port->Init.WordLength == UART_WORDLENGTH_8B ? "8" : (ta_Modbus_M[i].port->Init.WordLength == UART_WORDLENGTH_7B ? "7" : "9"));
		printf("\tParity bit: \t%s\n", ta_Modbus_M[i].port->Init.Parity == UART_PARITY_NONE ? "none" : (ta_Modbus_M[i].port->Init.Parity == UART_PARITY_EVEN ? "even" : "odd"));
		printf("\tStop bit: \t%s\n", ta_Modbus_M[i].port->Init.StopBits == UART_STOPBITS_1 ? "1" : (ta_Modbus_M[i].port->Init.StopBits == UART_STOPBITS_1_5 ? "1.5" : "2"));
	}

	/* Infinite loop */
	for (;;) {
		mmosal_task_sleep(1000);
	}
}
