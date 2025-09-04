/*
 * serial.c 
 *
 */

#include <stdio.h>
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "main.h"
#include "task.h"
#include "mmosal.h"
#include "serial.h"

#ifdef STM_USBD_ACM_ENABLE
#include "app_fifo.h"
#include "usbd_cdc_if.h"

extern struct mmosal_task *xConsoleHandle;
extern struct kfifo cdcRxFifo;
#else
#error "STM_USBD_ACM_ENABLE no define."
#endif

xComPortHandle xSerialPortInitMinimal(int baudRate, int queueLength)
{
	baudRate = baudRate;
	queueLength = queueLength;

	return 0x00;
}

BaseType_t xSerialGetChar(xComPortHandle xPort, signed char *cRxedChar, uint32_t timeout)
{
    UNUSED(xPort);

#ifdef STM_USBD_ACM_ENABLE
	static uint32_t i = 0x00;
#else
#error "STM_USBD_ACM_ENABLE no define."
#endif

#ifdef STM_USBD_ACM_ENABLE

	if (i == 0x00) {
		mmosal_task_wait_for_notification(timeout);

		i = kfifo_len(&cdcRxFifo);
	}

	if (0x01 == kfifo_get(&cdcRxFifo, (unsigned char*) cRxedChar, 0x01)) {
		i--;
	}

	return pdPASS;
#else
#error "STM_USBD_ACM_ENABLE no define."
#endif
}

BaseType_t vSerialPutString(xComPortHandle xPort, signed char *pMessage, unsigned short len)
{
	UNUSED(xPort);

#ifdef STM_USBD_ACM_ENABLE
	while(CDC_Transmit_FS((uint8_t*)pMessage, len) != USBD_OK) {
		mmosal_task_sleep(100);
	}
#else
#error "STM_USBD_ACM_ENABLE no define."
#endif
	return pdTRUE;
}

BaseType_t xSerialPutChar(xComPortHandle xPort, signed char c, uint32_t timeout)
{
	UNUSED(xPort);
	UNUSED(timeout);

	char tx = c;
	uint32_t start_time = mmosal_get_time_ms();
	const uint32_t timeout_ms = 50; // Very short timeout: 50ms
	
	
	// Try to send character with aggressive timeout
	while (true) {
#ifdef STM_USBD_ACM_ENABLE
		uint8_t result = CDC_Transmit_FS((uint8_t*)&tx, 0x01);
		if (result == USBD_OK) {
			break;
		}
#else
#error "STM_USBD_ACM_ENABLE no define."
#endif
		// Check timeout
		if ((mmosal_get_time_ms() - start_time) > timeout_ms) {
			break;
		}
		
		mmosal_task_sleep(1); // Very short sleep
	}

	return pdTRUE;
}

