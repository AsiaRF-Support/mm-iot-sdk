/*
 * Modbus.c
 *  Modbus RTU Master and Slave library for STM32 CUBE with FreeRTOS
 *  Created on: Feb 13, 2025
 *      Author: Leo Hsiao, Denny Hsieh, Elwin Huang
 *      Adapted from https://github.com/alejoseb/Modbus-STM32-HAL-FreeRTOS
 *
 *  SPDX-License-Identifier: LGPL-2.1
 *
 */

#include <FreeRTOS.h>
#include "cmsis_os2.h"
#include <task.h>
#include <queue.h>
#include <timers.h>
#include <semphr.h>

#include "Modbus.h"

#include "mmwlan.h"
#include "mmosal.h"
#include "mmhal.h"

#include "lwip/tcpip.h"

#if ENABLE_TCP == 1
#include <lwip/api.h>
#include <lwip/ip4_addr.h>
#include <lwip/netif.h>
#endif

#ifndef ENABLE_USART_DMA
#define ENABLE_USART_DMA 0
#endif

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))

#define lowByte(w) ((w) & 0xff)
#define highByte(w) ((w) >> 8)


modbusHandler_t *mHandlers[MAX_M_HANDLERS];


///Queue Modbus telegrams for master
const osMessageQueueAttr_t QueueTelegram_attributes = {
	.name = "QueueModbusTelegram"
};


//Semaphore to access the Modbus Data
const osSemaphoreAttr_t ModBusSphr_attributes = {
	.name = "ModBusSphr"
};

// Define RTOS priority from RTOS2 to FreeRTOS
#define PRIORITY_LOW        (tskIDLE_PRIORITY + 1) // osPriorityLow
#define PRIORITY_NORMAL     (tskIDLE_PRIORITY + 2) // osPriorityNormal
#define PRIORITY_HIGH       (tskIDLE_PRIORITY + 3) // osPriorityHigh
#define PRIORITY_REALTIME   (tskIDLE_PRIORITY + 4) // osPriorityRealtime

// Attributes for Modbus Task parameters
#define MODBUS_TELEGRAME_NAME			"QueMbusTelegram"
#define MODBUS_SEM_NAME					"ModBusSphr"

#define MODBUS_SLAVE_NAME               "TaskModbusSlave"
#define MODBUS_SLAVE_STACK_SIZE         (128 * 4)
#define MODBUS_SLAVE_TCP_STACK_SIZE     (256 * 8)

#define MODBUS_MASTER_NAME              "TaskModbusMaster"
#define MODBUS_MASTER_STACK_SIZE        (128 * 4)
#define MODBUS_MASTER_TCP_STACK_SIZE    (256 * 4)


uint8_t numberHandlers = 0;

static void sendTxBuffer(modbusHandler_t *modH);
static int16_t getRxBuffer(modbusHandler_t *modH);
static uint8_t validateAnswer(modbusHandler_t *modH);
static void buildException( uint8_t u8exception, modbusHandler_t *modH );
static uint8_t validateRequest(modbusHandler_t *modH);
static uint16_t word(uint8_t H, uint8_t l);
static void get_FC1(modbusHandler_t *modH);
static void get_FC3(modbusHandler_t *modH);
static int8_t process_FC1(modbusHandler_t *modH );
static int8_t process_FC3(modbusHandler_t *modH );
static int8_t process_FC5( modbusHandler_t *modH);
static int8_t process_FC6(modbusHandler_t *modH );
static int8_t process_FC15(modbusHandler_t *modH );
static int8_t process_FC16(modbusHandler_t *modH);
static void vTimerCallbackT35(TimerHandle_t pxTimer);
static void vTimerCallbackTimeout(TimerHandle_t pxTimer);
static int8_t SendQuery(modbusHandler_t *modH ,modbus_t telegram);
static bool isBufferValid(modbusHandler_t *modH);


#if ENABLE_TCP ==1

static bool TCPwaitConnData(modbusHandler_t *modH);
static void TCPinitserver(modbusHandler_t *modH);
static mb_errot_t TCPconnectserver(modbusHandler_t *modH, modbus_t *telegram);
static mb_errot_t TCPgetRxBuffer(modbusHandler_t *modH);

#endif


/* Ring Buffer functions */
// This function must be called only after disabling USART RX interrupt or inside of the RX interrupt
void RingAdd(modbusRingBuffer_t *xRingBuffer, uint8_t u8Val)
{

	xRingBuffer->uxBuffer[xRingBuffer->u8end] = u8Val;
	xRingBuffer->u8end = (xRingBuffer->u8end + 1) % MAX_BUFFER;
	if (xRingBuffer->available == MAX_BUFFER)
	{
		xRingBuffer->overflow = true;
		xRingBuffer->u8start = (xRingBuffer->u8start + 1) % MAX_BUFFER;
	}
	else
	{
		xRingBuffer->overflow = false;
		xRingBuffer->available++;
	}

}

// This function must be called only after disabling USART RX interrupt
uint8_t RingGetAllBytes(modbusRingBuffer_t *xRingBuffer, uint8_t *buffer)
{
	return RingGetNBytes(xRingBuffer, buffer, xRingBuffer->available);
}

// This function must be called only after disabling USART RX interrupt
uint8_t RingGetNBytes(modbusRingBuffer_t *xRingBuffer, uint8_t *buffer, uint16_t uNumber)
{
	uint8_t uCounter;
	if(xRingBuffer->available == 0  || uNumber == 0 ) return 0;
	if(uNumber > MAX_BUFFER) return 0;

	for(uCounter = 0; uCounter < uNumber && uCounter< xRingBuffer->available ; uCounter++)
	{
		buffer[uCounter] = xRingBuffer->uxBuffer[xRingBuffer->u8start];
		xRingBuffer->u8start = (xRingBuffer->u8start + 1) % MAX_BUFFER;
	}
	xRingBuffer->available = xRingBuffer->available - uCounter;
	xRingBuffer->overflow = false;
	RingClear(xRingBuffer);

	return uCounter;
}

uint8_t RingCountBytes(modbusRingBuffer_t *xRingBuffer)
{
	return xRingBuffer->available;
}

void RingClear(modbusRingBuffer_t *xRingBuffer)
{
	xRingBuffer->u8start = 0;
	xRingBuffer->u8end = 0;
	xRingBuffer->available = 0;
	xRingBuffer->overflow = false;
}

/* End of Ring Buffer functions */



const unsigned char fctsupported[] =
{
	MB_FC_READ_COILS,
	MB_FC_READ_DISCRETE_INPUT,
	MB_FC_READ_REGISTERS,
	MB_FC_READ_INPUT_REGISTER,
	MB_FC_WRITE_COIL,
	MB_FC_WRITE_REGISTER,
	MB_FC_WRITE_MULTIPLE_COILS,
	MB_FC_WRITE_MULTIPLE_REGISTERS
};

#ifdef ENABLE_DATA_LED

/**
 * @brief
 * Configure the TX LED for Modbus Master.
 * This function will create a timer to turn off the TX LED after 150 ms.
 *
 * @param modH         modbus handler
 * @param led_config   pointer to the LED configuration
 * @param off_callback callback function to turn off the LED
 */
void Modbus_Led_Configure_tx(modbusHandler_t *modH, led_ctrl_t *led_ctrl, void *off_callback)
{
	if (led_ctrl != NULL) {
		modH->tx_led = led_ctrl;
	} else {
		modH->tx_led = NULL;
		return;
	}

	modH->tx_led->xTimer_off_callback = xTimerCreate(
		"LED_OFF_Timer_TX",
		pdMS_TO_TICKS(150), // 150 ms
		pdFALSE,
		(void *)modH->tx_led,
		(TimerCallbackFunction_t) off_callback
	);

	if (modH->tx_led->xTimer_off_callback == NULL) {
		printf("Failed to create LED_OFF_Timer_TX for Modbus Master\n");
	}
}

/**
 * @brief
 * Configure the RX LED for Modbus.
 * This function will set the LED configuration and create a timer for turning off the LED.
 *
 * @param modH         modbus handler
 * @param led_config   pointer to the LED configuration structure
 * @param off_callback callback function to be called when the timer expires
 */
void Modbus_Led_Configure_rx(modbusHandler_t *modH, led_ctrl_t *led_ctrl, void *off_callback)
{
	
	if (led_ctrl != NULL) {
		modH->rx_led = led_ctrl;
	} else {
		modH->rx_led = NULL;
		return;
	}

	modH->rx_led->xTimer_off_callback = xTimerCreate(
		"LED_OFF_Timer_RX",
		pdMS_TO_TICKS(150), // 150 ms
		pdFALSE,
		(void *)modH->rx_led,
		(TimerCallbackFunction_t) off_callback
	);
	
	if (modH->rx_led->xTimer_off_callback == NULL) {
		printf("Failed to create LED_OFF_Timer_RX for Modbus Master\n");
	}
}
#endif

/**
 * @brief
 * Initialization for a Master config.
 * this function will add config parameters to Modbus master
 *
 * @param modH   modbus handler
 */
void Modbus_Master_Configure(modbusHandler_t *modH, uint16_t *Modbus_DATA, uint16_t Modbus_DATA_SIZE, UART_HandleTypeDef *huart, uint16_t Time_Out, GPIO_TypeDef *Flow_Ctr_Port, uint16_t Flow_Ctr_Mode, char *name)
{
	modH->master_ready_sem = mmosal_sem_create(2, 0, "master_sem");

	modH->uModbusType = MB_MASTER;
	modH->port = huart;						// This is not used for TCP
	strncpy(modH->mb_name, name, sizeof(modH->mb_name));
	modH->u8id = 0;							// Slave ID for master always 0
	modH->u16timeOut = Time_Out;			// For a master it could be higher depending on the slave speed
	modH->EN_Port = Flow_Ctr_Port;			// No RS485
	modH->EN_Pin = Flow_Ctr_Mode;			// No RS485
	//ModbusH2.EN_Port = LD2_GPIO_Port;		// RS485 Enable
	//ModbusH2.EN_Pin = LD2_Pin;			// RS485 Enable
	modH->u16regs = Modbus_DATA;
	modH->u16regsize = Modbus_DATA_SIZE;
	modH->xTypeHW = USART_HW;				// Serial USART hardware
}


/**
 * @brief
 * Initialization for a Master/Slave.
 * this function will add config parameters to Modbus slave
 *
 * @param modH   modbus handler
 */
void Modbus_Slave_Configure(modbusHandler_t *modH, uint16_t *Modbus_DATA, uint16_t Modbus_DATA_SIZE, uint8_t ID, uint16_t Time_Out, uint16_t TCP_Port, char *name)
{
	modH->slave_ready_semb = mmosal_semb_create("slave_sem");

	modH->uModbusType = MB_SLAVE;
	strncpy(modH->mb_name, name, sizeof(modH->mb_name));
	modH->u8id = ID;						// Slave ID, slave ID for master always 0
	modH->u16timeOut = Time_Out;
	//modH->EN_Port = NULL;					// No RS485
	//modH->EN_Pin = NULL;					// No RS485
	//ModbusH2.EN_Port = LD2_GPIO_Port;		// RS485 Enable
	//ModbusH2.EN_Pin = LD2_Pin;			// RS485 Enable
	modH->u16regs = Modbus_DATA;
	modH->u16regsize = Modbus_DATA_SIZE;
	modH->xTypeHW = TCP_HW;					// TCP hardware
	modH->uTcpPort = TCP_Port;				// used only for the slave (i.e., server)
}


/**
 * @brief
 * Initialization for a Master/Slave.
 * this function will check the configuration parameters
 * of the modbus handler
 *
 * @param modH   modbus handler
 */
void ModbusInit(modbusHandler_t *modH)
{
	if (numberHandlers < MAX_M_HANDLERS)
	{
		//Initialize the ring buffer

		RingClear(&modH->xBufferRX);

		if(modH->uModbusType == MB_SLAVE)
		{
			//Create Modbus task slave
#if ENABLE_TCP == 1
			if( modH->xTypeHW == TCP_HW)
			{
				modH->myTaskModbusAHandle = mmosal_task_create(
						StartTaskModbusSlave,
						(void *)modH,
						MMOSAL_TASK_PRI_LOW,
						MODBUS_SLAVE_TCP_STACK_SIZE,
						MODBUS_SLAVE_NAME
						);
			}
			else{
				modH->myTaskModbusAHandle = mmosal_task_create(
						StartTaskModbusSlave,
						(void *)modH,
						MMOSAL_TASK_PRI_LOW,
						MODBUS_SLAVE_STACK_SIZE,
						MODBUS_SLAVE_NAME
						);
			}
#else
			modH->myTaskModbusAHandle = mmosal_task_create(
					StartTaskModbusSlave,
					(void *)modH,
					MMOSAL_TASK_PRI_LOW,
					MODBUS_SLAVE_STACK_SIZE,
					MODBUS_SLAVE_NAME
					);
#endif


		}
		else if (modH->uModbusType == MB_MASTER)
		{
			//Create Modbus task Master  and Queue for telegrams

#if ENABLE_TCP == 1
			if( modH->xTypeHW == TCP_HW)
			{
				modH->myTaskModbusAHandle = mmosal_task_create(
						StartTaskModbusMaster,
						(void *)modH,
						MMOSAL_TASK_PRI_LOW,
						MODBUS_MASTER_TCP_STACK_SIZE,
						MODBUS_MASTER_NAME
						);
			}
			else
			{
				modH->myTaskModbusAHandle = mmosal_task_create(
						StartTaskModbusMaster,
						(void *)modH,
						MMOSAL_TASK_PRI_LOW,
						MODBUS_MASTER_STACK_SIZE,
						MODBUS_MASTER_NAME
						);
				modH->myTaskTelegramHandle = mmosal_task_create(
						StartTaskTelegram,
						(void *)modH,
						MMOSAL_TASK_PRI_LOW,
						MODBUS_MASTER_STACK_SIZE,
						MODBUS_MASTER_NAME
						);
			}
#else
			modH->myTaskModbusAHandle = mmosal_task_create(
					StartTaskModbusMaster,
					(void *)modH,
					MMOSAL_TASK_PRI_LOW,
					MODBUS_MASTER_STACK_SIZE,
					MODBUS_MASTER_NAME
					);
#endif

			modH->xTimerTimeout=xTimerCreate("xTimerTimeout",	// Just a text name, not used by the kernel.
					modH->u16timeOut ,							// The timer period in ticks.
					pdFALSE,									// The timers will auto-reload themselves when they expire.
					( void * )modH->xTimerTimeout,				// Assign each timer a unique id equal to its array index.
					(TimerCallbackFunction_t) vTimerCallbackTimeout		// Each timer calls the same callback when it expires.
					);

			if(modH->xTimerTimeout == NULL)
			{
				__ERR("Dead\n");
				while(1); //error creating timer, check heap and stack size
			}

			modH->QueueTelegramHandle = osMessageQueueNew (MAX_TELEGRAMS, sizeof(modbus_t), NULL);

			if(modH->QueueTelegramHandle == NULL)
			{
				__ERR("Dead\n");
				while(1); //error creating queue for telegrams, check heap and stack size
			}
		}
		else
		{
			__ERR("Dead\n");
			while(1); //Error Modbus type not supported choose a valid Type
		}

		if  (modH->myTaskModbusAHandle == NULL)
		{
			__ERR("Dead\n");
			while(1); //Error creating Modbus task, check heap and stack size
		}

		modH->xTimerT35 = xTimerCreate("TimerT35",		// Just a text name, not used by the kernel.
				T35,							// The timer period in ticks.
				pdFALSE,						// The timers will auto-reload themselves when they expire.
				( void * )modH->xTimerT35,		// Assign each timer a unique id equal to its array index.
				(TimerCallbackFunction_t) vTimerCallbackT35		// Each timer calls the same callback when it expires.
				);

		if (modH->xTimerT35 == NULL)
		{
			__ERR("Dead\n");
			while(1); //Error creating the timer, check heap and stack size
		}

		modH->ModBusSphrHandle = osSemaphoreNew(1, 1, NULL);

		if(modH->ModBusSphrHandle == NULL)
		{
			__ERR("Dead\n");
			while(1); //Error creating the semaphore, check heap and stack size
		}

		//Record both Master and Slave handlers
		mHandlers[numberHandlers] = modH;
		numberHandlers++;
	}
	else
	{
		__ERR("Dead\n");
		while(1); //error no more Modbus handlers supported
	}
}

/**
 * @brief
 * Start object.
 *
 * Call this AFTER calling begin() on the serial port, typically within setup().
 *
 * (If you call this function, then you should NOT call any of
 * ModbusRtu's own begin() functions.)
 *
 * @ingroup setup
 */
void ModbusStart(modbusHandler_t *modH)
{

	if(modH->xTypeHW != USART_HW && modH->xTypeHW != TCP_HW && modH->xTypeHW != USB_CDC_HW  && modH->xTypeHW != USART_HW_DMA )
	{
		__ERR("Dead\n");
		while(1); //ERROR select the type of hardware
	}

	if (modH->xTypeHW == USART_HW_DMA && ENABLE_USART_DMA == 0  )
	{
		__ERR("Dead\n");
		while(1); //ERROR To use USART_HW_DMA you need to enable it in the ModbusConfig.h file
	}

	if (modH->xTypeHW == USART_HW || modH->xTypeHW ==  USART_HW_DMA )
	{
		if (modH->EN_Port != NULL )
		{
			// return RS485 transceiver to transmit mode
			HAL_GPIO_WritePin(modH->EN_Port, modH->EN_Pin, GPIO_PIN_RESET);
		}

		if (modH->uModbusType == MB_SLAVE &&  modH->u16regs == NULL )
		{
			__ERR("Dead\n");
			while(1); //ERROR define the DATA pointer shared through Modbus
		}

		//check that port is initialized
		while (HAL_UART_GetState(modH->port) != HAL_UART_STATE_READY)
		{
			mmosal_task_sleep(10);
		}

#if ENABLE_USART_DMA ==1
		if( modH->xTypeHW == USART_HW_DMA )
		{
			if(HAL_UARTEx_ReceiveToIdle_DMA(modH->port, modH->xBufferRX.uxBuffer, MAX_BUFFER ) != HAL_OK)
			{
				__ERR("Dead\n");
				while(1)
				{
					//error in your initialization code
				}
			}
			__HAL_DMA_DISABLE_IT(modH->port->hdmarx, DMA_IT_HT); // we don't need half-transfer interrupt
		}
		else
		{
			// Receive data from serial port for Modbus using interrupt
			if(HAL_UART_Receive_IT(modH->port, &modH->dataRX, 1) != HAL_OK)
			{
				__ERR("Dead\n");
				while(1)
				{
					//error in your initialization code
				}
			}
		}
#else
		// Receive data from serial port for Modbus using interrupt
		if(HAL_UART_Receive_IT(modH->port, &modH->dataRX, 1) != HAL_OK)
		{
			__ERR("Dead\n");
			while(1)
			{
				//error in your initialization code
			}
		}
#endif
		if(modH->u8id !=0 && modH->uModbusType == MB_MASTER )
		{
			__ERR("Dead\n");
			while(1)
			{
				//error Master ID must be zero
			}
		}

		if(modH->u8id ==0 && modH->uModbusType == MB_SLAVE )
		{
			__ERR("Dead\n");
			while(1)
			{
				//error Master ID must be zero
			}
		}
	}

#if ENABLE_TCP == 1
	


#endif

	modH->u8lastRec = modH->u8BufferSize = 0;
	modH->u16InCnt = modH->u16OutCnt = modH->u16errCnt = 0;

	modH->buffer_last_update_time = 0;
}

#if ENABLE_USB_CDC == 1
extern void MX_USB_DEVICE_Init(void);
void ModbusStartCDC(modbusHandler_t *modH)
{
	if (modH->uModbusType == MB_SLAVE &&  modH->u16regs == NULL )
	{
		__ERR("Dead\n");
		while(1); //ERROR define the DATA pointer shared through Modbus
	}

	modH->u8lastRec = modH->u8BufferSize = 0;
	modH->u16InCnt = modH->u16OutCnt = modH->u16errCnt = 0;
}
#endif

void vTimerCallbackT35(TimerHandle_t pxTimer)
{
    // Notify that a stream has just arrived
    int i;

    for (i = 0; i < numberHandlers; i++)
    {
        if (mHandlers[i]->xTimerT35 == pxTimer) {
            if (mHandlers[i]->uModbusType == MB_MASTER)
            {
                xTimerStop(mHandlers[i]->xTimerTimeout, 0);
            }

#ifdef ENABLE_DATA_LED
            if (mHandlers[i]->rx_led != NULL) {
				// Flash Once
				led_on(&mHandlers[i]->rx_led->led_config);
                xTimerReset(mHandlers[i]->rx_led->xTimer_off_callback, 0);
            }
#endif

            xTaskNotify((TaskHandle_t)mHandlers[i]->myTaskModbusAHandle, 0, eSetValueWithOverwrite);
			if (mHandlers[i]->matcher->uModbusType == MB_SLAVE &&
				mHandlers[i]->session == mHandlers[i]->matcher->session) {
					// Update buffer timestamp
					mHandlers[i]->matcher->buffer_last_update_time = HAL_GetTick();
			}
        }
    }
}

void vTimerCallbackTimeout(TimerHandle_t pxTimer)
{
	//Notify that a stream has just arrived
	//TimerHandle_t aux;
	for(int i = 0; i < numberHandlers; i++)
	{
		if (mHandlers[i]->xTimerTimeout ==  pxTimer) {
			xTaskNotify((TaskHandle_t)mHandlers[i]->myTaskModbusAHandle, ERR_TIME_OUT, eSetValueWithOverwrite);
		}
	}
}

/**
 * @brief Check RS485 RX buffer validate by timestamp
 * @param modH modbus handler
 * @return true if buffer is fresh, false otherwise
 */
static bool isBufferValid(modbusHandler_t *modH)
{
	if (!modH) {
		return false;
	}

	//Check buffer age
	return ((HAL_GetTick() - modH->buffer_last_update_time) <= MODBUS_BUFFER_MAX_AGE_MS);
}


#if ENABLE_TCP ==1

bool TCPwaitConnData(modbusHandler_t *modH)
{
	struct netbuf *inbuf;
	err_t recv_err, accept_err;
	char* buf;
	uint16_t buflen;
	uint16_t uLength;
	bool xTCPvalid = false;
	tcpclients_t *clientconn;

	//select the next connection slot to work with using round-robin
	modH->newconnIndex++;
	if (modH->newconnIndex >= NUMBERTCPCONN)
	{
		modH->newconnIndex = 0;
	}
	clientconn = &modH->newconns[modH->newconnIndex];

	//NULL means there is a free connection slot, so we can accept an incoming client connection
	if (clientconn->conn == NULL)
	{
		/* accept any incoming connection */
		accept_err = netconn_accept(modH->conn, &clientconn->conn);
		if(accept_err != ERR_OK)
		{
			// not valid incoming connection at this time
			ModbusCloseConnNull(modH);

			return xTCPvalid;
		}
		else
		{
			clientconn->aging = 0;
			modH->buffer_last_update_time = HAL_GetTick();
		}
	}
	else {
		// Already link.
	}

	netconn_set_nonblocking(modH->conn, 0);
	netconn_set_recvtimeout(clientconn->conn, modH->u16timeOut);
	recv_err = netconn_recv(clientconn->conn, &inbuf);

	if (recv_err == ERR_CLSD) //the connection was closed
	{
		//Close and clean the connection
		ModbusCloseConnNull(modH);

		clientconn->aging = 0;
		return xTCPvalid;
	}

	if (recv_err == ERR_TIMEOUT) //No new data
	{
		//continue the aging process
		modH->newconns[modH->newconnIndex].aging++;

		// if the connection is old enough and inactive close and clean it up
		if (modH->newconns[modH->newconnIndex].aging > TCPAGINGCYCLES || mmwlan_get_sta_state() != MMWLAN_STA_CONNECTED)
		{
			ModbusCloseConnNull(modH);
			clientconn->aging = 0;
			return xTCPvalid;
		}
		printf("TCP connection timeout detected. Port: %d, Retry: %ld/%d\n",
				modH->session + 1,
				modH->newconns[modH->newconnIndex].aging,
				TCPAGINGCYCLES);

		return xTCPvalid;
	}

	if (recv_err == ERR_CONN) {
		ModbusCloseConnNull(modH);
		clientconn->aging = 0;
		return xTCPvalid;
	}
	if (recv_err == ERR_RST) {
		ModbusCloseConnNull(modH);
		clientconn->aging = 0;
		return xTCPvalid;
	}

	if (recv_err == ERR_OK)
	{
		if (netconn_err(clientconn->conn) == ERR_OK)
		{
			/* Read the data from the port, blocking if nothing yet there.
			   We assume the request (the part we care about) is in one netbuf */
			netbuf_data(inbuf, (void**)&buf, &buflen);
			if (buflen>11) // minimum frame size for modbus TCP
			{
				if(buf[2] == 0 || buf[3] == 0 ) //validate protocol ID
				{
					uLength = (buf[4]<<8 & 0xff00) | buf[5];
					if(uLength< (MAX_BUFFER-2)  && (uLength + 6) <= buflen)
					{
						for(int i = 0; i < uLength; i++)
						{
							modH->u8Buffer[i] = buf[i+6];
						}
						modH->u16TransactionID = (buf[0]<<8 & 0xff00) | buf[1];
						modH->u8BufferSize = uLength + 2; //add 2 dummy bytes for CRC
						xTCPvalid = true; // we have data for the modbus slave
					}
				}
			}
			netbuf_delete(inbuf); // delete the buffer always
			clientconn->aging = 0; //reset the aging counter
		}
	}

	return xTCPvalid;
}


void  TCPinitserver(modbusHandler_t *modH)
{
	err_t err;

	/* Create a new TCP connection handle */
	if(modH-> xTypeHW == TCP_HW)
	{
		modH->conn = netconn_new(NETCONN_TCP);

		if (modH->conn != NULL)
		{
			/* Bind to port (502) Modbus with default IP address */
			if(modH->uTcpPort == 0) {
				__MSG("modH port set to default 502\n");
				modH->uTcpPort = 502; //if port not defined
			}

			err = netconn_bind(modH->conn, IP_ADDR_ANY, modH->uTcpPort);

			if (err == ERR_OK)
			{
				/* Put the connection into LISTEN state */
				err = netconn_listen(modH->conn);
			}
			else{
				__ERR("Dead\n");
				while(1)
				{
					// error binding the TCP Modbus port check your configuration
				}
			}
		}
		else{
			__ERR("Dead\n");
			while(1)
			{
				// error creating new connection check your configuration,
				// this function must be called after the scheduler is started
			}
		}
	}
}
#endif


void StartTaskModbusSlave(void *argument)
{
	modbusHandler_t *modH =  (modbusHandler_t *)argument;

	mmosal_semb_wait(modH->slave_ready_semb, UINT32_MAX);

#if ENABLE_TCP ==1
	if( modH->xTypeHW == TCP_HW )
	{
		TCPinitserver(modH); // start the Modbus server slave
	}
#endif

	for(;;)
	{
		modH->i8lastError = 0;

#if ENABLE_USB_CDC ==1
		if(modH-> xTypeHW == USB_CDC_HW)
		{
			ulTaskNotifyTake(pdTRUE, portMAX_DELAY); /* Block indefinitely until a Modbus Frame arrives */
			if (modH->u8BufferSize == ERR_BUFF_OVERFLOW) // is this necessary?
			{
				modH->i8lastError = ERR_BUFF_OVERFLOW;
				modH->u16errCnt++;
				continue;
			}
		}
#endif

#if ENABLE_TCP == 1
		if(modH-> xTypeHW == TCP_HW)
		{
			if(TCPwaitConnData(modH) == false || !isBufferValid(modH)) // wait for connection and receive data
			{
				continue; // TCP package was not validated or buffer is stale
			}
		}
#endif

		if(modH->xTypeHW == USART_HW || modH->xTypeHW == USART_HW_DMA)
		{
			ulTaskNotifyTake(pdTRUE, portMAX_DELAY); /* Block until a Modbus Frame arrives */

			if (getRxBuffer(modH) == ERR_BUFF_OVERFLOW)
			{
				modH->i8lastError = ERR_BUFF_OVERFLOW;
				modH->u16errCnt++;
				continue;
			}
		}

		if (modH->u8BufferSize < 7)
		{
			//The size of the frame is invalid
			modH->i8lastError = ERR_BAD_SIZE;
			modH->u16errCnt++;

			continue;
		}

		//check broadcast mode
		modH->u8AddressMode = ADDRESS_NORMAL;
		if (modH->u8Buffer[ID] == ADDRESS_BROADCAST)
		{
			modH->u8AddressMode = ADDRESS_BROADCAST;
		}

		// check slave id
		if ( modH->u8Buffer[ID] !=  modH->u8id && modH->u8AddressMode != ADDRESS_BROADCAST)
		{

#if ENABLE_TCP == 0
			continue; // continue this is not for us
#else
			if(modH->xTypeHW != TCP_HW)
			{
				continue; //for Modbus TCP this is not validated, user should modify accordingly if needed
			}
#endif
		}

		// validate message: CRC, FCT, address and size
		int8_t u8exception = validateRequest(modH);

		if (u8exception > 0)
		{
			if (u8exception != ERR_TIME_OUT)
			{
				buildException(u8exception, modH);
				sendTxBuffer(modH);
			}
			modH->i8lastError = u8exception;
			//return u8exception

			continue;
		}

		modH->i8lastError = 0;

		// update telegram function
		modH->pta_Telegram[0].u8id = modH->u8Buffer[ ID ];
		modH->pta_Telegram[0].u8fct = modH->u8Buffer[ FUNC ];
		modH->pta_Telegram[0].u16RegAdd =  word(modH->u8Buffer[ ADD_HI ], modH->u8Buffer[ ADD_LO ]);
		modH->pta_Telegram[0].u16CoilsNo =  word(modH->u8Buffer[ NB_HI ], modH->u8Buffer[ NB_LO ]);

		xSemaphoreTake(modH->ModBusSphrHandle, portMAX_DELAY); //before processing the message get the semaphore

		// process message
		switch(modH->u8Buffer[ FUNC ] )
		{
			case MB_FC_READ_COILS:
			case MB_FC_READ_DISCRETE_INPUT:

				if (modH->u8AddressMode == ADDRESS_BROADCAST)
				{
					/* broadcast mode should ignore read function */
					break;
				}
				modH->i8state = process_FC1(modH);
				break;

			case MB_FC_READ_INPUT_REGISTER:
			case MB_FC_READ_REGISTERS :

				if (modH->u8AddressMode == ADDRESS_BROADCAST)
				{
					/* broadcast mode should ignore read function */
					break;
				}
				modH->i8state = process_FC3(modH);
				break;

			case MB_FC_WRITE_COIL:

				modH->i8state = process_FC5(modH);
				break;

			case MB_FC_WRITE_REGISTER :

				modH->i8state = process_FC6(modH);
				break;

			case MB_FC_WRITE_MULTIPLE_COILS:

				modH->i8state = process_FC15(modH);
				break;

			case MB_FC_WRITE_MULTIPLE_REGISTERS :

				modH->i8state = process_FC16(modH);
				break;

			default:
				break;
		}
		xSemaphoreGive(modH->ModBusSphrHandle); //Release the semaphore

		continue;
	}
}


void ModbusQuery(modbusHandler_t *modH, modbus_t telegram )
{
	//Add the telegram to the TX tail Queue of Modbus
	if (modH->uModbusType == MB_MASTER)
	{
		telegram.u32CurrentTask = (uint32_t *) osThreadGetId();
		xQueueSendToBack(modH->QueueTelegramHandle, &telegram, 0);
	}
	else{
		__ERR("Dead\n");
		while(1);	// error a slave cannot send queries as a master
	}
}



void ModbusQueryInject(modbusHandler_t *modH, modbus_t telegram )
{
	//Add the telegram to the TX head Queue of Modbus
	xQueueReset(modH->QueueTelegramHandle);

	telegram.u32CurrentTask = (uint32_t *) osThreadGetId();

	xQueueSendToFront(modH->QueueTelegramHandle, &telegram, 0);
}


#if ENABLE_TCP ==1
void ModbusCloseConn(struct netconn *conn)
{
	if(conn != NULL)
	{
		netconn_close(conn);
		netconn_delete(conn);
	}
}

void ModbusCloseConnNull(modbusHandler_t *modH)
{
	if(modH->newconns[modH->newconnIndex].conn != NULL)
	{
		netconn_close(modH->newconns[modH->newconnIndex].conn);
		netconn_delete(modH->newconns[modH->newconnIndex].conn);
		modH->newconns[modH->newconnIndex].conn = NULL;
	}
}
#endif


/**
 * @brief
 * *** Only Modbus Master ***
 * Generate a query to an slave with a modbus_t telegram structure
 * The Master must be in COM_IDLE mode. After it, its state would be COM_WAITING.
 * This method has to be called only in loop() section.
 *
 * @see modbus_t
 * @param modH  modbus handler
 * @param modbus_t  modbus telegram structure (id, fct, ...)
 * @ingroup loop
 */
int8_t SendQuery(modbusHandler_t *modH ,  modbus_t telegram )
{
	uint8_t u8regsno, u8bytesno;
	uint8_t  error = 0;

	xSemaphoreTake(modH->ModBusSphrHandle , portMAX_DELAY); //before processing the message get the semaphore

	if (modH->u8id!=0) error = ERR_NOT_MASTER;
	if (modH->i8state != COM_IDLE) error = ERR_POLLING ;
	if ((telegram.u8id==0) || (telegram.u8id>247)) error = ERR_BAD_SLAVE_ID;

	if(error)
	{
		modH->i8lastError = error;
		xSemaphoreGive(modH->ModBusSphrHandle);
		return error;
	}

	modH->u16regs = telegram.u16reg;

	// telegram header
	modH->u8Buffer[ ID ]         = telegram.u8id;
	modH->u8Buffer[ FUNC ]       = telegram.u8fct;
	modH->u8Buffer[ ADD_HI ]     = highByte(telegram.u16RegAdd );
	modH->u8Buffer[ ADD_LO ]     = lowByte( telegram.u16RegAdd );

	switch( telegram.u8fct )
	{
		case MB_FC_READ_COILS:
		case MB_FC_READ_DISCRETE_INPUT:
		case MB_FC_READ_REGISTERS:
		case MB_FC_READ_INPUT_REGISTER:

			modH->u8Buffer[ NB_HI ]      = highByte(telegram.u16CoilsNo );
			modH->u8Buffer[ NB_LO ]      = lowByte( telegram.u16CoilsNo );
			modH->u8BufferSize = 6;
			break;

		case MB_FC_WRITE_COIL:

			modH->u8Buffer[ NB_HI ]      = (( telegram.u16reg[0]> 0) ? 0xff : 0);
			modH->u8Buffer[ NB_LO ]      = 0;
			modH->u8BufferSize = 6;
			break;

		case MB_FC_WRITE_REGISTER:

			modH->u8Buffer[ NB_HI ]      = highByte( telegram.u16reg[0]);
			modH->u8Buffer[ NB_LO ]      = lowByte( telegram.u16reg[0]);
			modH->u8BufferSize = 6;
			break;

		case MB_FC_WRITE_MULTIPLE_COILS: // TODO: implement "sending coils"

			u8regsno = telegram.u16CoilsNo / 16;
			u8bytesno = u8regsno * 2;
			if ((telegram.u16CoilsNo % 16) != 0)
			{
				u8bytesno++;
				u8regsno++;
			}

			modH->u8Buffer[ NB_HI ]      = highByte(telegram.u16CoilsNo );
			modH->u8Buffer[ NB_LO ]      = lowByte( telegram.u16CoilsNo );
			modH->u8Buffer[ BYTE_CNT ]    = u8bytesno;
			modH->u8BufferSize = 7;

			for (uint16_t i = 0; i < u8bytesno; i++)
			{
				if(i % 2)
				{
					modH->u8Buffer[ modH->u8BufferSize ] = lowByte( telegram.u16reg[ i / 2 ] );
				}
				else
				{
					modH->u8Buffer[ modH->u8BufferSize ] = highByte( telegram.u16reg[ i / 2 ] );
				}
				modH->u8BufferSize++;
			}
			break;

		case MB_FC_WRITE_MULTIPLE_REGISTERS:

			modH->u8Buffer[ NB_HI ]		= highByte(telegram.u16CoilsNo );
			modH->u8Buffer[ NB_LO ]		= lowByte( telegram.u16CoilsNo );
			modH->u8Buffer[ BYTE_CNT ]	= (uint8_t) ( telegram.u16CoilsNo * 2 );
			modH->u8BufferSize = 7;

			for (uint16_t i = 0; i < telegram.u16CoilsNo; i++)
			{
				modH->u8Buffer[ modH->u8BufferSize ] = highByte( telegram.u16reg[ i ] );
				modH->u8BufferSize++;
				modH->u8Buffer[ modH->u8BufferSize ] = lowByte( telegram.u16reg[ i ] );
				modH->u8BufferSize++;
			}
			break;
	}

	sendTxBuffer(modH);

	xSemaphoreGive(modH->ModBusSphrHandle);

	modH->i8state = COM_WAITING;
	modH->i8lastError = 0;

	return 0;
}


#if ENABLE_TCP == 1
static  mb_errot_t TCPconnectserver(modbusHandler_t *modH, modbus_t *telegram)
{
	err_t err;
	tcpclients_t *clientconn;

	//select the current connection slot to work with
	clientconn = &modH->newconns[modH->newconnIndex];

	if(telegram->u8clientID >= NUMBERTCPCONN )
	{
		return ERR_BAD_TCP_ID;
	}

	// if the connection is null open a new connection
	if (clientconn->conn == NULL)
	{
		clientconn->conn = netconn_new(NETCONN_TCP);
		if (clientconn->conn  == NULL)
		{
			__ERR("Dead\n");
			while(1)
			{
				// error creating new connection check your configuration and heap size
			}
		}

		err = netconn_connect(clientconn->conn, (ip_addr_t *)&telegram->xIpAddress, telegram->u16Port);

		if (err  != ERR_OK )
		{
			ModbusCloseConnNull(modH);

			return ERR_TIME_OUT;
		}
	}
	return (mb_errot_t)ERR_OK;
}


static mb_errot_t TCPgetRxBuffer(modbusHandler_t *modH)
{

	struct netbuf *inbuf;
	err_t err = ERR_TIME_OUT;
	char* buf;
	uint16_t buflen;
	uint16_t uLength;

	tcpclients_t *clientconn;
	//select the current connection slot to work with
	clientconn = &modH->newconns[modH->newconnIndex];

	//netconn_set_recvtimeout(clientconn->conn, modH->u16timeOut);
	//netconn_set_nonblocking(clientconn->conn, 0);
	netconn_set_recvtimeout(clientconn->conn, UINT32_MAX);
	err = netconn_recv(clientconn->conn, &inbuf);

	uLength = 0;

	if (err == ERR_OK)
	{
		err = netconn_err(clientconn->conn) ;
		if (err == ERR_OK)
		{
			/* Read the data from the port, blocking if nothing yet there.
			   We assume the request (the part we care about) is in one netbuf */
			err = netbuf_data(inbuf, (void**)&buf, &buflen);
			if (err == ERR_OK )
			{
				if ((buflen > 11 && (modH->uModbusType == MB_SLAVE )) ||
						(buflen >= 10 && (modH->uModbusType == MB_MASTER))) // minimum frame size for modbus TCP
				{
					if(buf[2] == 0 || buf[3] == 0 ) //validate protocol ID
					{
						uLength = (buf[4]<<8 & 0xff00) | buf[5];
						if(uLength < (MAX_BUFFER-2) && (uLength + 6) <= buflen)
						{
							for(int i = 0; i < uLength; i++)
							{
								modH->u8Buffer[i] = buf[i + 6];
							}
							modH->u16TransactionID = (buf[0] << 8 & 0xff00) | buf[1];
							modH->u8BufferSize = uLength + 2; //include 2 dummy bytes for CRC
						}
					}
				}
			}	// netbuf_data
			netbuf_delete(inbuf); //delete the buffer always
		}
	}

	return (mb_errot_t)err;
}
#endif


void StartTaskTelegram(void *arg)
{
	int NotificationValue;
	modbusHandler_t *modH = (modbusHandler_t *)arg;

	modbusHandler_t *modH2 = NULL;

	mmosal_sem_wait(modH->master_ready_sem, portMAX_DELAY);

	if (modH->uModbusType == MB_MASTER) {
		modH->pta_Telegram[0].u8id = 1;
		modH->pta_Telegram[0].u8fct = MB_FC_READ_REGISTERS;
		modH->pta_Telegram[0].u16RegAdd = 0x0;
		modH->pta_Telegram[0].u16CoilsNo = 50;
		modH->pta_Telegram[0].u16reg = modH->u16regs;
		modH->pta_Telegram[0].u8clientID = 0;
	}

	for (int i=0; i<numberHandlers; i++) {
		if (modH->matcher->uModbusType == MB_SLAVE &&
			modH->session == modH->matcher->session) {
			modH2 = modH->matcher;
			break;
		}
	}

	for (;;) {
		int user = 0;
		for (int i = 0; i < NUMBERTCPCONN; i++) {
			if (modH2->newconns[i].conn != NULL) {
				user++;
			}
		}

		if (modH->uModbusType == MB_MASTER && user > 0) {

			ModbusQuery(modH, modH->pta_Telegram[0]);
			NotificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
			if (NotificationValue != ERR_OK_QUERY) {
				// handle error
				 modH->u16regs[1]++;
			}
		}
		mmosal_task_sleep(500 * 2);
	}
}


void StartTaskModbusMaster(void *argument)
{
	modbusHandler_t *modH =  (modbusHandler_t *)argument;
	uint32_t ulNotificationValue;
	modbus_t telegram;

	mmosal_sem_wait(modH->master_ready_sem, portMAX_DELAY);

	for(;;)
	{
		/*Wait indefinitely for a telegram to send */
		xQueueReceive(modH->QueueTelegramHandle, &telegram, portMAX_DELAY);

#if ENABLE_TCP == 1
		if(modH->xTypeHW == TCP_HW)
		{
			modH->newconnIndex = telegram.u8clientID;
			ulNotificationValue = TCPconnectserver( modH, &telegram);
			if(ulNotificationValue == ERR_OK)
			{
				SendQuery(modH, telegram);
				/* Block until a Modbus Frame arrives or query timeouts*/
				ulNotificationValue = TCPgetRxBuffer(modH); // TCP receives the data and the notification simultaneously since it is synchronous

				if (ulNotificationValue != ERR_OK) //close the TCP connection
				{
					ModbusCloseConnNull(modH);
				}
			}
			else
			{
				ModbusCloseConnNull(modH);
			}
		}
		else // send a query for USART and USB_CDC
		{
			SendQuery(modH, telegram);
			ulNotificationValue = ulTaskNotifyTake(pdTRUE, 1000);		//(StartTaskModbusMaster)
		}
#else
		if(modH->port->Init.BaudRate <= 19200)
			osDelay((int)(35000 / modH->port->Init.BaudRate) + 2);
		else
			mmosal_task_sleep(3);

		// This is the case for implementations with only USART support
		SendQuery(modH, telegram);
		/* Block indefinitely until a Modbus Frame arrives or query timeouts*/
		ulNotificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
#endif

		// notify the task the request timeout
		modH->i8lastError = 0;
		if (ulNotificationValue)
		{
			modH->i8state = COM_IDLE;
			modH->i8lastError = ERR_TIME_OUT;
			modH->u16errCnt++;
			xTaskNotify((TaskHandle_t)telegram.u32CurrentTask, modH->i8lastError, eSetValueWithOverwrite);
			continue;
		}

#if ENABLE_USB_CDC ==1 || ENABLE_TCP ==1

		if(modH->xTypeHW == USART_HW) //TCP and USB_CDC use different methods to get the buffer
		{
			getRxBuffer(modH);
		}

#else
		getRxBuffer(modH);
#endif

		if ( modH->u8BufferSize < 6){
			modH->i8state = COM_IDLE;
			modH->i8lastError = ERR_BAD_SIZE;
			modH->u16errCnt++;
			xTaskNotify((TaskHandle_t)telegram.u32CurrentTask, modH->i8lastError, eSetValueWithOverwrite);
			continue;
		}

		xTimerStop(modH->xTimerTimeout,0); // cancel timeout timer

		// validate message: id, CRC, FCT, exception
		int8_t u8exception = validateAnswer(modH);
		if (u8exception != 0)
		{
			modH->i8state = COM_IDLE;
			modH->i8lastError = u8exception;
			xTaskNotify((TaskHandle_t)telegram.u32CurrentTask, modH->i8lastError, eSetValueWithOverwrite);
			continue;
		}

		modH->i8lastError = u8exception;

		xSemaphoreTake(modH->ModBusSphrHandle , portMAX_DELAY); //before processing the message get the semaphore

		// process answer
		switch( modH->u8Buffer[ FUNC ] )
		{
			case MB_FC_READ_COILS:
			case MB_FC_READ_DISCRETE_INPUT:

				//call get_FC1 to transfer the incoming message to u16regs buffer
				get_FC1(modH);
				break;

			case MB_FC_READ_INPUT_REGISTER:
			case MB_FC_READ_REGISTERS:

				// call get_FC3 to transfer the incoming message to u16regs buffer
				get_FC3(modH);
				break;

			case MB_FC_WRITE_COIL:
			case MB_FC_WRITE_REGISTER:
			case MB_FC_WRITE_MULTIPLE_COILS:
			case MB_FC_WRITE_MULTIPLE_REGISTERS:

				// nothing to do
				break;

			default:
				break;
		}
		modH->i8state = COM_IDLE;

		if (modH->i8lastError == 0) // no error the error_OK, we need to use a different value than 0 to detect the timeout
		{
			xSemaphoreGive(modH->ModBusSphrHandle); //Release the semaphore

			xTaskNotify((TaskHandle_t)telegram.u32CurrentTask, ERR_OK_QUERY, eSetValueWithOverwrite);
		}
		continue;
	}
}

/**
 * This method processes functions 1 & 2 (for master)
 * This method puts the slave answer into master data buffer
 *
 * @ingroup register
 */
void get_FC1(modbusHandler_t *modH)
{
	uint8_t u8byte, i;
	u8byte = 3;

	for (i = 0; i < modH->u8Buffer[2]; i++) {
		if(i % 2)
			modH->u16regs[i / 2]= word(modH->u8Buffer[i + u8byte], lowByte(modH->u16regs[i / 2]));
		else
			modH->u16regs[i / 2]= word(highByte(modH->u16regs[i / 2]), modH->u8Buffer[i + u8byte]);
	}
}

/**
 * This method processes functions 3 & 4 (for master)
 * This method puts the slave answer into master data buffer
 *
 * @ingroup register
 */
void get_FC3(modbusHandler_t *modH)
{
	uint16_t u16StartAddr = modH->pta_Telegram[0].u16RegAdd;
	uint16_t u8regsno = modH->pta_Telegram[0].u16CoilsNo;
	uint8_t u8byte, i;
	u8byte = 3;

	for (i = 0; i < u8regsno; i++)
	{
		modH->u16regs[ i + u16StartAddr ] = word(modH->u8Buffer[ u8byte ], modH->u8Buffer[ u8byte + 1 ]);
		u8byte += 2;
	}
}


/**
 * @brief
 * This method validates master incoming messages
 *
 * @return 0 if OK, EXCEPTION if anything fails
 * @ingroup buffer
 */
uint8_t validateAnswer(modbusHandler_t *modH)
{
	// check message crc vs calculated crc

#if ENABLE_TCP ==1
	if(modH->xTypeHW != TCP_HW)
	{
#endif
		uint16_t u16MsgCRC =
			((modH->u8Buffer[modH->u8BufferSize - 2] << 8)
			 | modH->u8Buffer[modH->u8BufferSize - 1]); // combine the crc Low & High bytes
		if ( calcCRC(modH->u8Buffer,  modH->u8BufferSize - 2) != u16MsgCRC )
		{
			modH->u16errCnt ++;
			return ERR_BAD_CRC;
		}
#if ENABLE_TCP ==1
	}
#endif

	// check exception
	if ((modH->u8Buffer[ FUNC ] & 0x80) != 0)
	{
		modH->u16errCnt ++;
		return ERR_EXCEPTION;
	}

	// check fct code
	bool isSupported = false;
	for (uint8_t i = 0; i< sizeof( fctsupported ); i++)
	{
		if (fctsupported[i] == modH->u8Buffer[FUNC])
		{
			isSupported = 1;
			break;
		}
	}
	if (!isSupported)
	{
		modH->u16errCnt ++;
		return EXC_FUNC_CODE;
	}

	return 0; // OK, no exception code thrown
}


/**
 * @brief
 * This method moves Serial buffer data to the Modbus u8Buffer.
 *
 * @return buffer size if OK, ERR_BUFF_OVERFLOW if u8BufferSize >= MAX_BUFFER
 * @ingroup buffer
 */
int16_t getRxBuffer(modbusHandler_t *modH)
{
	int16_t i16result;

	if(modH->xTypeHW == USART_HW)
	{
		HAL_UART_AbortReceive_IT(modH->port); // disable interrupts to avoid race conditions on serial port
	}

	if (modH->xBufferRX.overflow)
	{
		RingClear(&modH->xBufferRX); // clean up the overflowed buffer
		i16result =  ERR_BUFF_OVERFLOW;
	}
	else
	{
		modH->u8BufferSize = RingGetAllBytes(&modH->xBufferRX, modH->u8Buffer);
		modH->u16InCnt++;
		i16result = modH->u8BufferSize;
	}

	if(modH->xTypeHW == USART_HW)
	{
		HAL_UART_Receive_IT(modH->port, &modH->dataRX, 1);
	}

	return i16result;
}



/**
 * @brief
 * This method validates slave incoming messages
 *
 * @return 0 if OK, EXCEPTION if anything fails
 * @ingroup modH Modbus handler
 */
uint8_t validateRequest(modbusHandler_t *modH)
{
	// check message crc vs calculated crc

#if ENABLE_TCP ==1
	uint16_t u16MsgCRC;
	u16MsgCRC = ((modH->u8Buffer[modH->u8BufferSize - 2] << 8)
			| modH->u8Buffer[modH->u8BufferSize - 1]); // combine the crc Low & High bytes

	if (modH->xTypeHW != TCP_HW)
	{
		if ( calcCRC( modH->u8Buffer,  modH->u8BufferSize-2 ) != u16MsgCRC )
		{
			modH->u16errCnt ++;
			return ERR_BAD_CRC;
		}
	}
#else
	uint16_t u16MsgCRC;
	u16MsgCRC = ((modH->u8Buffer[modH->u8BufferSize - 2] << 8)
			| modH->u8Buffer[modH->u8BufferSize - 1]); // combine the crc Low & High bytes

	if ( calcCRC( modH->u8Buffer,  modH->u8BufferSize-2 ) != u16MsgCRC )
	{
		modH->u16errCnt ++;
		return ERR_BAD_CRC;
	}
#endif

	// check fct code
	bool isSupported = false;
	for (uint8_t i = 0; i < sizeof( fctsupported ); i++)
	{
		if (fctsupported[i] == modH->u8Buffer[FUNC])
		{
			isSupported = 1;
			break;
		}
	}
	if (!isSupported)
	{
		modH->u16errCnt ++;
		return EXC_FUNC_CODE;
	}

	// check start address & nb range
	uint16_t u16AdRegs = 0;
	uint16_t u16NRegs = 0;

	switch ( modH->u8Buffer[ FUNC ] )
	{
		case MB_FC_READ_COILS:
		case MB_FC_READ_DISCRETE_INPUT:
		case MB_FC_WRITE_MULTIPLE_COILS:

			u16AdRegs = word( modH->u8Buffer[ ADD_HI ], modH->u8Buffer[ ADD_LO ]) / 16;
			u16NRegs = word( modH->u8Buffer[ NB_HI ], modH->u8Buffer[ NB_LO ]) / 16;
			if(word( modH->u8Buffer[ NB_HI ], modH->u8Buffer[ NB_LO ]) % 16) u16NRegs++; // check for incomplete words
			// verify address range
			if((u16AdRegs + u16NRegs) > modH->u16regsize) return EXC_ADDR_RANGE;

			//verify answer frame size in bytes

			u16NRegs = word( modH->u8Buffer[ NB_HI ], modH->u8Buffer[ NB_LO ]) / 8;
			if(word( modH->u8Buffer[ NB_HI ], modH->u8Buffer[ NB_LO ]) % 8) 
				u16NRegs++;
			u16NRegs = u16NRegs + 5; // adding the header  and CRC ( Slave address + Function code  + number of data bytes to follow + 2-byte CRC )
			if(u16NRegs > 256) return EXC_REGS_QUANT;

			break;
		case MB_FC_WRITE_COIL:

			u16AdRegs = word( modH->u8Buffer[ ADD_HI ], modH->u8Buffer[ ADD_LO ]) / 16;
			if(word( modH->u8Buffer[ ADD_HI ], modH->u8Buffer[ ADD_LO ]) % 16)
				u16AdRegs++;	// check for incomplete words
			if (u16AdRegs > modH->u16regsize)
				return EXC_ADDR_RANGE;
			break;

		case MB_FC_WRITE_REGISTER:

			u16AdRegs = word( modH->u8Buffer[ ADD_HI ], modH->u8Buffer[ ADD_LO ]);
			if (u16AdRegs > modH-> u16regsize)
				return EXC_ADDR_RANGE;
			break;

		case MB_FC_READ_REGISTERS:
		case MB_FC_READ_INPUT_REGISTER:
		case MB_FC_WRITE_MULTIPLE_REGISTERS:

			u16AdRegs = word( modH->u8Buffer[ ADD_HI ], modH->u8Buffer[ ADD_LO ]);
			u16NRegs = word( modH->u8Buffer[ NB_HI ], modH->u8Buffer[ NB_LO ]);
			if (( u16AdRegs + u16NRegs ) > modH->u16regsize)
				return EXC_ADDR_RANGE;

			//verify answer frame size in bytes
			u16NRegs = u16NRegs * 2 + 5; // adding the header  and CRC
			if ( u16NRegs > 256 )
				return EXC_REGS_QUANT;
			break;
	}
	return 0; // OK, no exception code thrown
}


/**
 * @brief
 * This method creates a word from 2 bytes
 *
 * @return uint16_t (word)
 * @ingroup H  Most significant byte
 * @ingroup L  Less significant byte
 */
uint16_t word(uint8_t H, uint8_t L)
{
	bytesFields W;
	W.u8[0] = L;
	W.u8[1] = H;

	return W.u16[0];
}


/**
 * @brief
 * This method calculates CRC
 *
 * @return uint16_t calculated CRC value for the message
 * @ingroup Buffer
 * @ingroup u8length
 */
uint16_t calcCRC(uint8_t *Buffer, uint8_t u8length)
{
	unsigned int temp, temp2, flag;
	temp = 0xFFFF;
	for (unsigned char i = 0; i < u8length; i++)
	{
		temp = temp ^ Buffer[i];
		for (unsigned char j = 1; j <= 8; j++)
		{
			flag = temp & 0x0001;
			temp >>=1;
			if (flag)
				temp ^= 0xA001;
		}
	}
	// Reverse byte order.
	temp2 = temp >> 8;
	temp = (temp << 8) | temp2;
	temp &= 0xFFFF;
	// the returned value is already swapped
	// crcLo byte is first & crcHi byte is last
	return temp;
}


/**
 * @brief
 * This method builds an exception message
 *
 * @ingroup u8exception exception number
 * @ingroup modH modbus handler
 */
void buildException( uint8_t u8exception, modbusHandler_t *modH )
{
	uint8_t u8func = modH->u8Buffer[ FUNC ];  // get the original FUNC code

	modH->u8Buffer[ ID ]	= modH->u8id;
	modH->u8Buffer[ FUNC ]	= u8func + 0x80;
	modH->u8Buffer[ 2 ]		= u8exception;
	modH->u8BufferSize		= EXCEPTION_SIZE;
}


#if ENABLE_USB_CDC == 1
extern uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);
#endif

/**
 * @brief
 * This method transmits u8Buffer to Serial line.
 * Only if u8txenpin != 0, there is a flow handling in order to keep
 * the RS485 transceiver in output state as long as the message is being sent.
 * This is done with TC bit.
 * The CRC is appended to the buffer before starting to send it.
 *
 * @return nothing
 * @ingroup modH Modbus handler
 */
static void sendTxBuffer(modbusHandler_t *modH)
{
	// when in slaveType and u8AddressMode == ADDRESS_BROADCAST, do not send anything
	if (modH->uModbusType == MB_SLAVE && modH->u8AddressMode == ADDRESS_BROADCAST)
	{
		modH->u8BufferSize = 0;
		// increase message counter
		modH->u16OutCnt++;
		return;
	}

	// append CRC to message

#if  ENABLE_TCP == 1
	if(modH->xTypeHW != TCP_HW)
	{
#endif

		uint16_t u16crc = calcCRC(modH->u8Buffer, modH->u8BufferSize);
		modH->u8Buffer[ modH->u8BufferSize ] = u16crc >> 8;
		modH->u8BufferSize++;
		modH->u8Buffer[ modH->u8BufferSize ] = u16crc & 0x00ff;
		modH->u8BufferSize++;

#if ENABLE_TCP == 1
	}
#endif

#if ENABLE_USB_CDC == 1 || ENABLE_TCP == 1
	if(modH->xTypeHW == USART_HW || modH->xTypeHW == USART_HW_DMA )
	{
#endif

		if (modH->EN_Port != NULL)
		{
			//enable transmitter, disable receiver to avoid echo on RS485 transceivers
			HAL_HalfDuplex_EnableTransmitter(modH->port);
			HAL_GPIO_WritePin(modH->EN_Port, modH->EN_Pin, GPIO_PIN_SET);
		}

#if ENABLE_USART_DMA ==1
		if(modH->xTypeHW == USART_HW)
		{
#endif

			// transfer buffer to serial line IT
			HAL_UART_Transmit_IT(modH->port, modH->u8Buffer,  modH->u8BufferSize);

#ifdef ENABLE_DATA_LED
			if(modH->tx_led != NULL)
			{
				// Flash Once
				led_on(&modH->tx_led->led_config);
				xTimerReset(modH->tx_led->xTimer_off_callback, 0);
			}
#endif

#if ENABLE_USART_DMA ==1
		}
		else
		{
			//transfer buffer to serial line DMA
			HAL_UART_Transmit_DMA(modH->port, modH->u8Buffer, modH->u8BufferSize);
		}
#endif

		ulTaskNotifyTake(pdTRUE, 250); //wait notification from TXE interrupt
		/*
		 * If you are porting the library to a different MCU check the 
		 * USART datasheet and add the corresponding family in the following
		 * preprocessor conditions
		 */
#if defined(STM32H7)  || defined(STM32F3) || defined(STM32L4) || defined(STM32L082xx) || defined(STM32F7) || defined(STM32WB) || defined(STM32G070xx) || defined(STM32F0) || defined(STM32G431xx)
		while((modH->port->Instance->ISR & USART_ISR_TC) == 0)
#else
		// F429, F103, L152 ...
		while((modH->port->Instance->ISR & USART_ISR_TC) ==0)
#endif
		{
			__ERR("looping\n");
			//block the task until the the last byte is send out of the shifting buffer in USART
		}

		if (modH->EN_Port != NULL)
		{
			//return RS485 transceiver to receive mode
			HAL_GPIO_WritePin(modH->EN_Port, modH->EN_Pin, GPIO_PIN_RESET);
			//enable receiver, disable transmitter
			HAL_HalfDuplex_EnableReceiver(modH->port);
		}

		// set timeout for master query
		if(modH->uModbusType == MB_MASTER )
		{
			xTimerReset(modH->xTimerTimeout, 0);
		}
#if ENABLE_USB_CDC == 1 || ENABLE_TCP == 1
	}

#if ENABLE_USB_CDC == 1
	else if(modH->xTypeHW == USB_CDC_HW)
	{
		CDC_Transmit_FS(modH->u8Buffer,  modH->u8BufferSize);
		// set timeout for master query
		if(modH->uModbusType == MB_MASTER )
		{
			xTimerReset(modH->xTimerTimeout, 0);
		}
	}
#endif

#if ENABLE_TCP == 1

	else if(modH->xTypeHW == TCP_HW)
	{
		struct netvector  xNetVectors[2];
		uint8_t u8MBAPheader[6];
		size_t uBytesWritten;

		u8MBAPheader[0] = highByte(modH->u16TransactionID); // this might need improvement the transaction ID could be validated
		u8MBAPheader[1] = lowByte(modH->u16TransactionID);
		u8MBAPheader[2] = 0; //protocol ID
		u8MBAPheader[3] = 0; //protocol ID
		u8MBAPheader[4] = 0; //highbyte data length always 0
		u8MBAPheader[5] = modH->u8BufferSize; //highbyte data length

		xNetVectors[0].len = 6;
		xNetVectors[0].ptr = (void *) u8MBAPheader;

		xNetVectors[1].len = modH->u8BufferSize;
		xNetVectors[1].ptr = (void *) modH->u8Buffer;

		netconn_set_sendtimeout(modH->newconns[modH->newconnIndex].conn, modH->u16timeOut);
		err_enum_t err;

		err = netconn_write_vectors_partly(modH->newconns[modH->newconnIndex].conn, xNetVectors, 2, NETCONN_COPY, &uBytesWritten);
		if (err != ERR_OK )
		{
			ModbusCloseConnNull(modH);
		}

		if(modH->uModbusType == MB_MASTER )
		{
			xTimerReset(modH->xTimerTimeout, 0);
		}
	}

#endif

#endif

	modH->u8BufferSize = 0;
	// increase message counter
	modH->u16OutCnt++;
}


/**
 * @brief
 * This method processes functions 1 & 2
 * This method reads a bit array and transfers it to the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup discrete
 */
int8_t process_FC1(modbusHandler_t *modH )
{
	uint16_t u16currentRegister;
	uint8_t u8currentBit, u8bytesno, u8bitsno;
	uint8_t u8CopyBufferSize;
	uint16_t u16currentCoil, u16coil;

	// get the first and last coil from the message
	uint16_t u16StartCoil = word( modH->u8Buffer[ ADD_HI ], modH->u8Buffer[ ADD_LO ] );
	uint16_t u16Coilno = word( modH->u8Buffer[ NB_HI ], modH->u8Buffer[ NB_LO ] );

	// put the number of bytes in the outcoming message
	u8bytesno = (uint8_t) (u16Coilno / 8);
	if (u16Coilno % 8 != 0)
		u8bytesno ++;
	modH->u8Buffer[ ADD_HI ]	= u8bytesno;
	modH->u8BufferSize			= ADD_LO;
	modH->u8Buffer[modH->u8BufferSize + u8bytesno - 1 ] = 0;

	// read each coil from the register map and put its value inside the outcoming message
	u8bitsno = 0;

	for (u16currentCoil = 0; u16currentCoil < u16Coilno; u16currentCoil++)
	{
		u16coil = u16StartCoil + u16currentCoil;
		u16currentRegister =  (u16coil / 16);
		u8currentBit = (uint8_t) (u16coil % 16);

		bitWrite(
				modH->u8Buffer[ modH->u8BufferSize ],
				u8bitsno,
				bitRead( modH->u16regs[ u16currentRegister ], u8currentBit )
			);
		u8bitsno ++;

		if (u8bitsno > 7)
		{
			u8bitsno = 0;
			modH->u8BufferSize++;
		}
	}

	// send outcoming message
	if (u16Coilno % 8 != 0) modH->u8BufferSize ++;
	u8CopyBufferSize = modH->u8BufferSize + 2;
	sendTxBuffer(modH);
	return u8CopyBufferSize;
}


/**
 * @brief
 * This method processes functions 3 & 4
 * This method reads a word array and transfers it to the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup register
 */
int8_t process_FC3(modbusHandler_t *modH)
{
	uint16_t u16StartAdd = word( modH->u8Buffer[ ADD_HI ], modH->u8Buffer[ ADD_LO ] );
	uint8_t u8regsno = word( modH->u8Buffer[ NB_HI ], modH->u8Buffer[ NB_LO ] );
	uint8_t u8CopyBufferSize;
	uint16_t i;

	modH->u8Buffer[2] = u8regsno * 2;
	modH->u8BufferSize = 3;

	for (i = u16StartAdd; i < u16StartAdd + u8regsno; i++)
	{
		modH->u8Buffer[ modH->u8BufferSize ] = highByte(modH->u16regs[i]);
		modH->u8BufferSize++;
		modH->u8Buffer[ modH->u8BufferSize ] = lowByte(modH->u16regs[i]);
		modH->u8BufferSize++;
	}
	u8CopyBufferSize = modH->u8BufferSize +2;
	sendTxBuffer(modH);

	return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 5
 * This method writes a value assigned by the master to a single bit
 *
 * @return u8BufferSize Response to master length
 * @ingroup discrete
 */
int8_t process_FC5( modbusHandler_t *modH )
{
	uint8_t u8currentBit;
	uint16_t u16currentRegister;
	uint8_t u8CopyBufferSize;
	uint16_t u16coil = word( modH->u8Buffer[ ADD_HI ], modH->u8Buffer[ ADD_LO ] );

	// point to the register and its bit
	u16currentRegister = (u16coil / 16);
	u8currentBit = (uint8_t) (u16coil % 16);

	// write to coil
	bitWrite(
			modH->u16regs[ u16currentRegister ],
			u8currentBit,
			modH->u8Buffer[ NB_HI ] == 0xff
		);


	// send answer to master
	modH->u8BufferSize = 6;
	u8CopyBufferSize =  modH->u8BufferSize + 2;
	sendTxBuffer(modH);

	return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 6
 * This method writes a value assigned by the master to a single word
 *
 * @return u8BufferSize Response to master length
 * @ingroup register
 */
int8_t process_FC6(modbusHandler_t *modH )
{
	uint16_t u16add = word( modH->u8Buffer[ ADD_HI ], modH->u8Buffer[ ADD_LO ] );
	uint8_t u8CopyBufferSize;
	uint16_t u16val = word( modH->u8Buffer[ NB_HI ], modH->u8Buffer[ NB_LO ] );

	modH->u16regs[ u16add ] = u16val;

	// keep the same header
	modH->u8BufferSize = RESPONSE_SIZE;

	u8CopyBufferSize = modH->u8BufferSize + 2;
	sendTxBuffer(modH);

	return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 15
 * This method writes a bit array assigned by the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup discrete
 */
int8_t process_FC15( modbusHandler_t *modH )
{
	uint8_t u8currentBit, u8frameByte, u8bitsno;
	uint16_t u16currentRegister;
	uint8_t u8CopyBufferSize;
	uint16_t u16currentCoil, u16coil;
	bool bTemp;

	// get the first and last coil from the message
	uint16_t u16StartCoil = word( modH->u8Buffer[ ADD_HI ], modH->u8Buffer[ ADD_LO ] );
	uint16_t u16Coilno = word( modH->u8Buffer[ NB_HI ], modH->u8Buffer[ NB_LO ] );


	// read each coil from the register map and put its value inside the outcoming message
	u8bitsno = 0;
	u8frameByte = 7;
	for (u16currentCoil = 0; u16currentCoil < u16Coilno; u16currentCoil++)
	{

		u16coil = u16StartCoil + u16currentCoil;
		u16currentRegister = (u16coil / 16);
		u8currentBit = (uint8_t) (u16coil % 16);

		bTemp = bitRead(
				modH->u8Buffer[ u8frameByte ],
				u8bitsno );

		bitWrite(
				modH->u16regs[ u16currentRegister ],
				u8currentBit,
				bTemp );

		u8bitsno ++;

		if (u8bitsno > 7)
		{
			u8bitsno = 0;
			u8frameByte++;
		}
	}

	// send outcoming message
	// it's just a copy of the incomping frame until 6th byte
	modH->u8BufferSize	= 6;
	u8CopyBufferSize	= modH->u8BufferSize + 2;
	sendTxBuffer(modH);
	return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 16
 * This method writes a word array assigned by the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup register
 */
int8_t process_FC16(modbusHandler_t *modH )
{
	uint16_t u16StartAdd = modH->u8Buffer[ ADD_HI ] << 8 | modH->u8Buffer[ ADD_LO ];
	uint16_t u16regsno = modH->u8Buffer[ NB_HI ] << 8 | modH->u8Buffer[ NB_LO ];
	uint8_t u8CopyBufferSize;
	uint16_t i;
	uint16_t temp;

	// build header
	modH->u8Buffer[ NB_HI ]		= 0;
	modH->u8Buffer[ NB_LO ]		= (uint8_t) u16regsno; // answer is always 256 or less bytes
	modH->u8BufferSize			= RESPONSE_SIZE;

	// write registers
	for (i = 0; i < u16regsno; i++)
	{
		temp = word(
				modH->u8Buffer[ (BYTE_CNT + 1) + i * 2 ],
				modH->u8Buffer[ (BYTE_CNT + 2) + i * 2 ]);

		modH->u16regs[ u16StartAdd + i ] = temp;
	}
	u8CopyBufferSize = modH->u8BufferSize + 2;
	sendTxBuffer(modH);

	return u8CopyBufferSize;
}


