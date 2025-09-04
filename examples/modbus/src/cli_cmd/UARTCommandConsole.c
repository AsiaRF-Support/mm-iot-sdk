/*
   FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
   All rights reserved

   VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

   This file is part of the FreeRTOS distribution.

   FreeRTOS is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License (version 2) as published by the
   Free Software Foundation >>>> AND MODIFIED BY <<<< the FreeRTOS exception.

 ***************************************************************************
 >>!   NOTE: The modification to the GPL is included to allow you to     !<<
 >>!   distribute a combined work that includes FreeRTOS without being   !<<
 >>!   obliged to provide the source code for proprietary components     !<<
 >>!   outside of the FreeRTOS kernel.                                   !<<
 ***************************************************************************

 FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  Full license text is available on the following
link: http://www.freertos.org/a00114.html

 ***************************************************************************
 *                                                                       *
 *    FreeRTOS provides completely free yet professionally developed,    *
 *    robust, strictly quality controlled, supported, and cross          *
 *    platform software that is more than just the market leader, it     *
 *    is the industry's de facto standard.                               *
 *                                                                       *
 *    Help yourself get started quickly while simultaneously helping     *
 *    to support the FreeRTOS project by purchasing a FreeRTOS           *
 *    tutorial book, reference manual, or both:                          *
 *    http://www.FreeRTOS.org/Documentation                              *
 *                                                                       *
 ***************************************************************************

http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
the FAQ page "My application does not run, what could be wrong?".  Have you
defined configASSERT()?

http://www.FreeRTOS.org/support - In return for receiving this top quality
embedded software for free we request you assist our global community by
participating in the support forum.

http://www.FreeRTOS.org/training - Investing in training allows your team to
be as productive as possible as early as possible.  Now you can receive
FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
Ltd, and the world's leading authority on the world's leading RTOS.

http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
including FreeRTOS+Trace - an indispensable productivity tool, a DOS
compatible FAT file system, and our tiny thread aware UDP/IP stack.

http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.

http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
licenses offer ticketed support, indemnification and commercial middleware.

http://www.SafeRTOS.com - High Integrity Systems also provide a safety
engineered and independently SIL3 certified version for use in safety and
mission critical applications that require provable dependability.

1 tab == 4 spaces!
 */

/*
 * NOTE:  This file uses CDC driver.
 */

/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <stddef.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Example includes. */
#include "FreeRTOS_CLI.h"

/* Demo application includes. */
#include "serial.h"

/* STM32 source */
#include "stm32u5xx.h"

/* Morse FreeRTOS API */
#include "mmosal.h"

#ifdef STM_USBD_ACM_ENABLE
/* USB source */
#include "usbd_def.h"
#else
#error "STM_USBD_ACM_ENABLE no define."
#endif




/* ------------------------------ Define ------------------------------ */

#define WELCOME_MSG_DELAY		pdMS_TO_TICKS(100)

/* Dimensions the buffer into which input characters are placed. */
#define cmdMAX_INPUT_SIZE		128

/* ASCII codes for special keys */
#define ASCII_ESC				0x1B
#define ASCII_ARROW_UP			0x41
#define ASCII_ARROW_DOWN		0x42
#define ASCII_ARROW_RIGHT		0x43
#define ASCII_ARROW_LEFT		0x44
#define cmdASCII_DEL			( 0x7F )
#define cmdASCII_ESC			( 0x1B )

/* Dimentions a buffer to be used by the UART driver, if the UART driver uses a buffer at all. */
#define cmdQUEUE_LENGTH			25

/* The maximum time to wait for the mutex that guards the UART to become available. */
#define cli_MUTEX_TIMEOUT_MS	300

#ifndef configCLI_BAUD_RATE
#define configCLI_BAUD_RATE		115200
#endif

#define ENABLE_ECHO_CMD			0



/* ------------------------------ Variables ------------------------------ */

/* Welcome message task */
struct mmosal_task *xWelcomeTask = NULL;
struct mmosal_semb *xWelcomeSemaphore = NULL;
struct mmosal_task *xConsoleHandle = NULL; // For USBD rx

/* Used to guard access to the UART in case messages are sent to the UART from more than one task. */
static struct mmosal_mutex *xTxMutex = NULL;

/* Cursor position and arrow key handling */
static unsigned char ucCursorPos = 0;  // Current cursor position in the input string
static unsigned char ucEscapeState = 0; // State machine for escape sequences (0=normal, 1=ESC, 2=ESC[, 3=ESC[n, 4=ESCO)
static unsigned char ucEscapeNumber = 0; // Store the number in escape sequences like ESC[3~

/* The handle to the UART port, which is not used by all ports. */
static xComPortHandle xPort = 0;

/* Const messages output by the command console. */
static const char * const pcNewLine = "\r\n";
static const char * const pcEndOfOutputMessage = "\n\r\n> ";
static const char * const pcEndOutput = "> ";
static const char * const pcWelcomeMessage = \
											 "\r\n\r\n"
											 "<<------------------------------------------------------>>\r\n"
											 "<<             AsiaRF Command Line Interface            >>\r\n"
											 "<<  Type 'help' to view the list of available commands  >>\r\n"
											 "<<------------------------------------------------------>>\r\n\r\n> ";

#ifdef STM_USBD_ACM_ENABLE
/**/
extern USBD_HandleTypeDef hUsbDeviceFS;
#endif


/* ------------------------------ Define ------------------------------ */

static void prvWelcomeMessageTask( void *pvParameters );

/*
 * The task that implements the command console processing.
*/
static void prvUARTCommandConsoleTask( void *pvParameters );
void vUARTCommandConsoleStart();


/*
 * Cursor movement and line editing functions
*/
static void handle_cursor_left(void);
static void handle_cursor_right(char *cInputString, unsigned char ucInputIndex);
static void insert_character_at_cursor(char *cInputString, unsigned char *pucInputIndex, char cChar);
static void delete_character_at_cursor(char *cInputString, unsigned char *pucInputIndex);
static void handle_delete_key(char *cInputString, unsigned char *pucInputIndex);
static void handle_home_key(void);
static void handle_end_key(unsigned char ucInputIndex);

#ifdef STM_USBD_ACM_ENABLE
static void vOutputString( const char * pcMessage );
#endif



/* ------------------------------ Functions ------------------------------ */

/* Function implementations for cursor movement and character insertion */
static void handle_cursor_left(void)
{
	if (ucCursorPos > 0) {
		ucCursorPos--;
		// Send cursor left sequence to terminal
#ifdef STM_USBD_ACM_ENABLE
		vSerialPutString( xPort, ( signed char * ) "\033[D", ( unsigned short ) strlen( "\033[D" ) );
#else
#error "STM_USBD_ACM_ENABLE no define."
#endif
	}
}

static void handle_cursor_right(char *cInputString, unsigned char ucInputIndex)
{
	// Suppress unused parameter warning
	(void)cInputString;

	if (ucCursorPos < ucInputIndex) {
		ucCursorPos++;
		// Send cursor right sequence to terminal
#ifdef STM_USBD_ACM_ENABLE
		vSerialPutString( xPort, ( signed char * ) "\033[C", ( unsigned short ) strlen( "\033[C" ) );
#else 
#error "STM_USBD_ACM_ENABLE no define."
#endif
	}
}

static void insert_character_at_cursor(char *cInputString, unsigned char *pucInputIndex, char cChar)
{
	if (*pucInputIndex < cmdMAX_INPUT_SIZE - 1) {
		// Move characters after cursor position to the right
		for (int i = *pucInputIndex; i > ucCursorPos; i--) {
			cInputString[i] = cInputString[i-1];
		}
		// Insert new character at cursor position
		cInputString[ucCursorPos] = cChar;
		(*pucInputIndex)++;
		ucCursorPos++;

#ifdef STM_USBD_ACM_ENABLE
		// For simple case at end of line, just echo the character
		if (ucCursorPos == *pucInputIndex) {
			// Simple case: inserting at end of line
			xSerialPutChar(xPort, (signed char) cChar, (uint32_t)portMAX_DELAY);
		} else {
			// Complex case: inserting in middle of line, need to redraw
			int chars_after_cursor = *pucInputIndex - ucCursorPos;
			// Echo the new character and all characters after it
			vSerialPutString( xPort, ( signed char * ) &cInputString[ucCursorPos-1], chars_after_cursor + 1 );
			// Move cursor back to correct position
			for (int i = 0; i < chars_after_cursor; i++) {
				vSerialPutString( xPort, ( signed char * ) "\033[D", ( unsigned short ) strlen( "\033[D" ) );
			}
		}
#else
#error "STM_USBD_ACM_ENABLE no define."
#endif
	}
}

static void delete_character_at_cursor(char *cInputString, unsigned char *pucInputIndex)
{
	if (ucCursorPos > 0 && *pucInputIndex > 0) {
		ucCursorPos--;
		(*pucInputIndex)--;

		// Move characters after cursor position to the left
		for (int i = ucCursorPos; i < *pucInputIndex; i++) {
			cInputString[i] = cInputString[i+1];
		}
		cInputString[*pucInputIndex] = '\0';

#ifdef STM_USBD_ACM_ENABLE
		// Redraw the line: go back one, then print remaining chars + space, then go back
		vSerialPutString( xPort, ( signed char * ) "\b", ( unsigned short ) strlen( "\b" ) );
		int chars_after_cursor = *pucInputIndex - ucCursorPos;
		if (chars_after_cursor > 0) {
			vSerialPutString( xPort, ( signed char * ) &cInputString[ucCursorPos], chars_after_cursor);
		}
		vSerialPutString( xPort, ( signed char * ) " \b", ( unsigned short ) strlen( " \b" ) );
		// Move cursor back to correct position
		for (int i = 0; i < chars_after_cursor; i++) {
			vSerialPutString( xPort, ( signed char * ) "\033[D", ( unsigned short ) strlen( "\033[D" ) );
		}
#else
#error "STM_USBD_ACM_ENABLE no define."
#endif
	}
}

/* Delete key: delete character at cursor position (not before cursor) */
static void handle_delete_key(char *cInputString, unsigned char *pucInputIndex)
{
	if (ucCursorPos < *pucInputIndex && *pucInputIndex > 0) {
		// Move characters after cursor position to the left
		for (int i = ucCursorPos; i < *pucInputIndex - 1; i++) {
			cInputString[i] = cInputString[i+1];
		}
		(*pucInputIndex)--;
		cInputString[*pucInputIndex] = '\0';

#ifdef STM_USBD_ACM_ENABLE
		// Redraw the line from cursor position
		int chars_after_cursor = *pucInputIndex - ucCursorPos;
		if (chars_after_cursor > 0) {
			vSerialPutString( xPort, ( signed char * ) &cInputString[ucCursorPos], chars_after_cursor);
		}
		vSerialPutString( xPort, ( signed char * ) " \b", ( unsigned short ) strlen( " \b" ) );

		// Move cursor back to correct position
		for (int i = 0; i < chars_after_cursor; i++) {
			vSerialPutString( xPort, ( signed char * ) "\033[D", ( unsigned short ) strlen( "\033[D" ) );
		}
#else
#error "STM_USBD_ACM_ENABLE no define."
#endif
	}
}

/* Home key: move cursor to beginning of line */
static void handle_home_key(void)
{
	while (ucCursorPos > 0) {
		ucCursorPos--;
#ifdef STM_USBD_ACM_ENABLE
	vSerialPutString( xPort, ( signed char * ) "\033[D", ( unsigned short ) strlen( "\033[D" ) );
}
#else
#error "STM_USBD_ACM_ENABLE no define."
#endif
}

/* End key: move cursor to end of line */
static void handle_end_key(unsigned char ucInputIndex)
{
	while (ucCursorPos < ucInputIndex) {
		ucCursorPos++;
#ifdef STM_USBD_ACM_ENABLE
	vSerialPutString( xPort, ( signed char * ) "\033[C", ( unsigned short ) strlen( "\033[C" ) );
}
#else
#error "STM_USBD_ACM_ENABLE no define."
#endif
}


void vUARTCommandConsoleStart()
{
	/* Create the semaphore used to access the UART Tx. */
	xTxMutex= mmosal_mutex_create("cli");
	MMOSAL_ASSERT(xTxMutex != NULL);

	/* Create the counting semaphore used to signal that characters have been
	   received. We use counting semaphore to avoid losing signals when multiple
	   characters arrive quickly. */

	xConsoleHandle = mmosal_task_create
		(
		 prvUARTCommandConsoleTask,		/* The task that implements the command console. */
		 NULL,							/* The parameter is not used, so NULL is passed. */
		 MMOSAL_TASK_PRI_LOW,			/* The priority allocated to the task. */
		 configMINIMAL_STACK_SIZE * 2,	/* The size of the stack allocated to the task. */
		 "CFG"							/* Text name assigned to the task.  This is just to assist debugging.  The kernel does not use this name itself. */
		);

	MMOSAL_ASSERT(xConsoleHandle != NULL);

	// Initialize welcome message task and semaphore
	xWelcomeSemaphore = mmosal_semb_create("Welecome_semb");
	MMOSAL_ASSERT(xWelcomeSemaphore != NULL);

	if (xWelcomeTask == NULL && xWelcomeSemaphore != NULL) {
		xWelcomeTask = mmosal_task_create(
				prvWelcomeMessageTask,          // Task function
				(void *)NULL,
				MMOSAL_TASK_PRI_LOW,            // Priority (lower than USB task)
				configMINIMAL_STACK_SIZE,		// Stack size
				"UARTWelcomeMsg"				// Task name
				);
		MMOSAL_ASSERT(xWelcomeTask != NULL);
	}
}

static void prvUARTCommandConsoleTask( void *pvParameters )
{
	signed char cRxedChar;
	uint8_t ucInputIndex = 0;
	char *pcOutputString;

#if (ENABLE_ECHO_CMD == 1)
	static char cInputString[ cmdMAX_INPUT_SIZE ], cLastInputString[ cmdMAX_INPUT_SIZE ];
#else
	static char cInputString[ cmdMAX_INPUT_SIZE ];
#endif

	BaseType_t xReturned;

	UNUSED(pvParameters);

	/* Obtain the address of the output buffer.  Note there is no mutual
	   exclusion on this buffer as it is assumed only one command console interface
	   will be used at any one time. */
	pcOutputString = FreeRTOS_CLIGetOutputBuffer();	/* Initialise the UART. */
	xPort = xSerialPortInitMinimal( configCLI_BAUD_RATE, cmdQUEUE_LENGTH );

#ifdef STM_USBD_ACM_ENABLE
	/* Wait USB CDC serial ready */
	while(hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) {
		mmosal_task_sleep(1000);
	}

	vOutputString(pcWelcomeMessage);
#endif

	for( ;; )
	{
#ifdef STM_USBD_ACM_ENABLE
		/* Wait for the next character.  The while loop is used in case
		   INCLUDE_vTaskSuspend is not set to 1 - in which case portMAX_DELAY will
		   be a genuine block time rather than an infinite block time. */
		while(xSerialGetChar(xPort, &cRxedChar, (uint32_t) portMAX_DELAY ) != pdPASS){};
#else
#error "STM_USBD_ACM_ENABLE no define."
#endif

			/* Ensure exclusive access to the UART Tx. */
			if (mmosal_mutex_get(xTxMutex, cli_MUTEX_TIMEOUT_MS))
			{
#ifdef STM_USBD_ACM_ENABLE
				/* Was it the end of the line? */
				if(cRxedChar == '\r'|| cRxedChar == '\n')
				{
					/* Just to space the output from the input. */
					vSerialPutString( xPort, ( signed char * ) pcNewLine, ( unsigned short ) strlen( pcNewLine ) );
#else
#error "STM_USBD_ACM_ENABLE no define."
#endif

					// Reset cursor position for command processing
					ucCursorPos = ucInputIndex;
					if( ucInputIndex != 0 ) {
						do
						{
							/* Get the next output string from the command interpreter. */
							xReturned = FreeRTOS_CLIProcessCommand( cInputString, pcOutputString, configCOMMAND_INT_MAX_OUTPUT_SIZE );
#ifdef STM_USBD_ACM_ENABLE
							vSerialPutString( xPort, ( signed char * ) pcOutputString, ( unsigned short ) strlen(pcOutputString) );
#else
#error "STM_USBD_ACM_ENABLE no define."
#endif
						} while( xReturned != pdFALSE );

						ucInputIndex = 0;
						ucCursorPos = 0; // Reset cursor position
						memset( cInputString, 0x00, cmdMAX_INPUT_SIZE );

						vSerialPutString( xPort, ( signed char * ) pcEndOfOutputMessage, ( unsigned short ) strlen( pcEndOfOutputMessage ) );
						memset(pcOutputString, 0x00, configCOMMAND_INT_MAX_OUTPUT_SIZE);
					}
					else {
						vSerialPutString( xPort, ( signed char * ) pcEndOutput, ( unsigned short ) strlen( pcEndOutput ) );
						memset(pcOutputString, 0x00, configCOMMAND_INT_MAX_OUTPUT_SIZE);
					}
				}
				else {					// Handle escape sequences for arrow keys and other special keys
					if (cRxedChar == cmdASCII_ESC) {
						ucEscapeState = 1; // Start of escape sequence
						ucEscapeNumber = 0;
					}
					else if (ucEscapeState == 1 && cRxedChar == '[') {
						ucEscapeState = 2; // ESC[ received, waiting for more
					}
					else if (ucEscapeState == 1 && cRxedChar == 'O') {
						ucEscapeState = 4; // ESC O received, waiting for function key
					}
					else if (ucEscapeState == 2) {
						if (cRxedChar >= '0' && cRxedChar <= '9') {
							// Number in escape sequence (like ESC[3~)
							ucEscapeNumber = ucEscapeNumber * 10 + (cRxedChar - '0');
							ucEscapeState = 3; // ESC[n received, waiting for ~
						}
						else if (cRxedChar == 'C') {
							// Right arrow key (ESC[C)
							handle_cursor_right(cInputString, ucInputIndex);
							ucEscapeState = 0;
						}
						else if (cRxedChar == 'D') {
							// Left arrow key (ESC[D)
							handle_cursor_left();
							ucEscapeState = 0;
						}
						else if (cRxedChar == 'H') {
							// Home key (ESC[H)
							handle_home_key();
							ucEscapeState = 0;
						}
						else if (cRxedChar == 'F') {
							// End key (ESC[F)
							handle_end_key(ucInputIndex);
							ucEscapeState = 0;
						}
						else {
							ucEscapeState = 0; // Reset for unknown keys
						}
					}
					else if (ucEscapeState == 3 && cRxedChar == '~') {
						// Complete escape sequence like ESC[3~
						if (ucEscapeNumber == 3) {
						// Delete key (ESC[3~)
								handle_delete_key(cInputString, &ucInputIndex);
						}
						else if (ucEscapeNumber == 1) {
							// Home key (ESC[1~)
							handle_home_key();
						}
						else if (ucEscapeNumber == 4) {
							// End key (ESC[4~)
							handle_end_key(ucInputIndex);
						}
						ucEscapeState = 0;
						ucEscapeNumber = 0;
					}
					else if (ucEscapeState == 4) {
						// ESC O sequences (function keys)
						if (cRxedChar == 'H') {
							// Home key (ESC O H)
							handle_home_key();
						}
						else if (cRxedChar == 'F') {
							// End key (ESC O F)
							handle_end_key(ucInputIndex);
						}
						ucEscapeState = 0;
					}
					else if( cRxedChar == '\r' ) {
						/* Ignore carriage return - it's already converted to \n in ISR */
						ucEscapeState = 0; // Reset escape state
						ucEscapeNumber = 0;
					}
					else if( ( cRxedChar == '\b' ) || ( cRxedChar == cmdASCII_DEL ) ) {
						/* Backspace was pressed. Use cursor-aware deletion. */
						delete_character_at_cursor(cInputString, &ucInputIndex);
						ucEscapeState = 0; // Reset escape state
						ucEscapeNumber = 0;
					}
					else {
						/* A character was entered. Insert it at cursor position. */
						if( ( cRxedChar >= ' ' ) && ( cRxedChar <= '~' ) ) {
							insert_character_at_cursor(cInputString, &ucInputIndex, cRxedChar);
						}
						ucEscapeState = 0; // Reset escape state
						ucEscapeNumber = 0;
					}
				}
				/* Must ensure to give the mutex back. */
				mmosal_mutex_release(xTxMutex);
			}
	}
}


static void prvWelcomeMessageTask( void *pvParameters )
{
	UNUSED(pvParameters);

	while (1) {
		// Wait for semaphore signal from tud_cdc_line_state_cb
		if (mmosal_semb_wait(xWelcomeSemaphore, portMAX_DELAY) == pdTRUE) {

			// Add 500ms delay as requested to ensure message integrity
			mmosal_task_sleep(WELCOME_MSG_DELAY);

			// Check if CDC is still connected before sending
#ifdef STM_USBD_ACM_ENABLE
			if ((hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED) && pcWelcomeMessage) {
				vOutputString(pcWelcomeMessage);
			}
#else
#error "STM_USBD_ACM_ENABLE no define."
#endif
		}
	}
}

#ifdef STM_USBD_ACM_ENABLE
static void vOutputString( const char * pcMessage )
{
	if (mmosal_mutex_get(xTxMutex, cli_MUTEX_TIMEOUT_MS)) {
		vSerialPutString( xPort, ( signed char * ) pcMessage, ( unsigned short ) strlen( pcMessage ));
		mmosal_mutex_release(xTxMutex);
	}
}
#endif
