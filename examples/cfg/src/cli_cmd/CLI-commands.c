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


/******************************************************************************
 *
 * http://www.FreeRTOS.org/cli
 *
 ******************************************************************************/


/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#ifdef STM_USBD_ACM_ENABLE
/* USBD includes */
#include "usbd_cdc_if.h"
#include "usb_device.h"
#else
#error "STM_USBD_ACM_ENABLE no define."
#endif

/* STM32 source */
#include "stm32u5xx.h"
//#include "stm32u5xx_ll_utils.h"

/* Morse source */
#include "mmwlan.h"
#include "mmconfig.h"
#include "mmutils.h"
#include "mmhal.h"
#include "mm_app_loadconfig.h" // Add for scan  function

#include "main.h"
#include "CLI-commands.h"
#include "serial.h"


#define FLASH_FW_ADDR				0x08008000UL

#if 0
#ifndef  configINCLUDE_TRACE_RELATED_CLI_COMMANDS
#define configINCLUDE_TRACE_RELATED_CLI_COMMANDS 0
#endif

#ifndef configINCLUDE_QUERY_HEAP_COMMAND
#define configINCLUDE_QUERY_HEAP_COMMAND 0
#endif
#endif


/*-----------------------------------------------------------*/


static xComPortHandle xPort = (xComPortHandle)0;

#ifdef STM_USBD_ACM_ENABLE
extern USBD_HandleTypeDef hUsbDeviceFS;
#endif

/* Helper to get CLI_CMD_E from command string */
static int get_CLI_CMD_INDEX_by_string(const char *cmd, size_t len) {
	/* Check length */
	if (len <= 0 || len >= MAX_CMD_LEN) {
		return -1; /* invalid */
	}

	/* Check character */
	if (cmd == NULL) {
		return -1; /* invalid */
	}

	for (int i = 0; i < CLI_CMD_COUNT; i++) {
		if (strncmp(cmd, CLI_CMD_MAP[i].cmd_str, len) == 0 && strlen(CLI_CMD_MAP[i].cmd_str) == len)
			return CLI_CMD_MAP[i].cmd_enum;
	}

	return -1;
}
/*
static const char *get_cli_cmd_str_by_index(const int i) {
	if (i < 0 || i >= CLI_CMD_COUNT)
		return "";

	return CLI_CMD_MAP[i].cmd_str;
}

static const char *get_cli_conf_key_by_index(const int i) { 
	if (i < 0 || i >= CLI_CMD_COUNT)
		return "";

	return CLI_CMD_MAP[i].conf_key;
}
*/
typedef struct {
	char ssid[MAX_SSID_LEN];
	char password[MAX_PASSWORD_LEN];
	char security[8];
	char country[4];
	bool dhcp;
	char ipaddr[MAX_ADDR_STR_LEN];
	char netmask[MAX_ADDR_STR_LEN];
	char gateway[MAX_ADDR_STR_LEN];
	char powersave[BUFFER_SIZE_SHORT]; // disabled = 8
} CLI_INFO_t;

typedef struct {
	char baudrate[BUFFER_SIZE_SHORT];
	char data_bit[BUFFER_SIZE_SHORT];
	char parity_bit[BUFFER_SIZE_SHORT];
	char stop_bit[BUFFER_SIZE_SHORT];
	char modbus_port[BUFFER_SIZE_SHORT];
} CLI_SERIAL_INFO_t;


/* The function that registers the commands that are defined within this file. */
void vRegisterSampleCLICommands( void );

/* Implements the task-stats command. */
//static BaseType_t prvTaskStatsCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );

#if( configGENERATE_RUN_TIME_STATS == 1 )
/* Implements the run-time-stats command. */
static BaseType_t prvRunTimeStatsCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
#endif /* configGENERATE_RUN_TIME_STATS */

#if 0
/* Implements the echo-three-parameters command. */
static BaseType_t prvThreeParameterEchoCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );

/* Implements the echo-parameters command. */
static BaseType_t prvParameterEchoCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
#endif

#if( configINCLUDE_QUERY_HEAP_COMMAND == 1 )
/* Implements the "query heap" command. */
static BaseType_t prvQueryHeapCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
#endif

#if( configINCLUDE_TRACE_RELATED_CLI_COMMANDS == 1 )
/* Implements the "trace start" and "trace stop" commands. */
static BaseType_t prvStartStopTraceCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
#endif

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )
/* Structure that defines the "task-stats" command line command.  This generates
   a table that gives information on each task in the system. */
static const CLI_Command_Definition_t xTaskStats =
{
	"task-stats", /* The command string to type. */
	"\r\ntask-stats:\r\n Displays a table showing the state of each FreeRTOS task\r\n",
	prvTaskStatsCommand, /* The function to run. */
	0 /* No parameters are expected. */
};
#endif


/* Reboot function. */
static BaseType_t prvReboot_CMD( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
#if 0
static void Jump_to_address(uint32_t address);
typedef void (*pFunction)(void);
static pFunction Jump_To;
static uint32_t JumpAddress;
#endif

/* Show info function. */
static BaseType_t prvInfo_CMD( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );

/* Reset function. */
static BaseType_t prvReset_CMD( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );

/* Scan function. */
static BaseType_t prvScan_CMD( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
static struct mmosal_semb *scan_finish_semb;

/* Get config function. */
static BaseType_t prvGet_PARM_CMD( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );

/* Set config function. */
static BaseType_t prvSet_PARM_CMD( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );


#if 0
/* Structure that defines the "echo_3_parameters" command line command.  This
   takes exactly three parameters that the command simply echos back one at a
   time. */
static const CLI_Command_Definition_t xThreeParameterEcho =
{
	"echo-3-parameters",
	"\r\necho-3-parameters <param1> <param2> <param3>:\r\n Expects three parameters, echos each in turn\r\n",
	prvThreeParameterEchoCommand, /* The function to run. */
	3 /* Three parameters are expected, which can take any value. */
};

/* Structure that defines the "echo_parameters" command line command.  This
   takes a variable number of parameters that the command simply echos back one at
   a time. */
static const CLI_Command_Definition_t xParameterEcho =
{
	"echo-parameters",
	"\r\necho-parameters <...>:\r\n Take variable number of parameters, echos each in turn\r\n",
	prvParameterEchoCommand, /* The function to run. */
	-1 /* The user can enter any number of commands. */
};
#endif

static const CLI_Command_Definition_t xReboot_CMD =
{
	"reboot", \
	" reboot\t\t\t : Reboot the device\r\n", \
	prvReboot_CMD, \
	0
};

static const CLI_Command_Definition_t xInfo_CMD =
{
	"info", \
	" info\t\t\t : Get configured information\r\n", \
	prvInfo_CMD, \
	0
};

static const CLI_Command_Definition_t xReset_CMD =
{
	"default", \
	" default\t\t : Reset all settings to default\r\n", \
	prvReset_CMD, \
	0
};

static const CLI_Command_Definition_t xScan_CMD =
{
	"scan", \
	" scan\t\t\t : Scan HaLow AP\r\n", \
	prvScan_CMD, \
	0
};

/*
static const CLI_Command_Definition_t xSet_Conf_CMD =
{
	"set", \
	CLI_CMD_MAP[i].help_str, \
	prvSet_PARM_CMD, \
	CLI_CMD_MAP[i].param_counts
};
*/

/* Helper macro to define CLI_Command_Definition_t objects for each command */
#define SET_CMD_REGISTER(ID) \
	static const CLI_Command_Definition_t xSet_##ID##_CMD = { \
		"set", \
		CLI_CMD_MAP[ID].help_str, \
		prvSet_PARM_CMD, \
		-1 \
	}

#define GET_CMD_REGISTER(ID) \
	static const CLI_Command_Definition_t xGet_##ID##_CMD = { \
		"get", \
		CLI_CMD_MAP[ID].help_str, \
		prvGet_PARM_CMD, \
		1 \
	}

/* Define command objects for each CLI command */
SET_CMD_REGISTER(CLI_SET_SSID);
SET_CMD_REGISTER(CLI_SET_PASSWORD);
SET_CMD_REGISTER(CLI_SET_SECURITY);
#if ENABLE_CHANGE_COUNTRY
SET_CMD_REGISTER(CLI_SET_COUNTRY);
#endif
SET_CMD_REGISTER(CLI_SET_DHCP);
SET_CMD_REGISTER(CLI_SET_IP);
SET_CMD_REGISTER(CLI_SET_NETMASK);
SET_CMD_REGISTER(CLI_SET_GATEWAY);
SET_CMD_REGISTER(CLI_SET_POWERSAVE);
#if MODBUS_NUM != 0
SET_CMD_REGISTER(CLI_SET_BAUDRATE);
SET_CMD_REGISTER(CLI_SET_DATA_BIT);
SET_CMD_REGISTER(CLI_SET_PARITY_BIT);
SET_CMD_REGISTER(CLI_SET_STOP_BIT);
SET_CMD_REGISTER(CLI_SET_MODBUS_PORT);
#endif
GET_CMD_REGISTER(CLI_GET_RSSI);

/* Array of pointers to all CLI command definitions for easy registration */
static const CLI_Command_Definition_t * const REGISTER_CLI_COMMAND[] = {
	&xReboot_CMD,
	&xInfo_CMD,
	&xReset_CMD,
	&xGet_CLI_GET_RSSI_CMD,
	&xScan_CMD,
	&xSet_CLI_SET_SSID_CMD,
	&xSet_CLI_SET_PASSWORD_CMD,
	&xSet_CLI_SET_SECURITY_CMD,
#if ENABLE_CHANGE_COUNTRY
	&xSet_CLI_SET_COUNTRY_CMD,
#endif
	&xSet_CLI_SET_DHCP_CMD,
	&xSet_CLI_SET_IP_CMD,
	&xSet_CLI_SET_NETMASK_CMD,
	&xSet_CLI_SET_GATEWAY_CMD,
	&xSet_CLI_SET_POWERSAVE_CMD,
#if MODBUS_NUM != 0
	&xSet_CLI_SET_BAUDRATE_CMD,
	&xSet_CLI_SET_DATA_BIT_CMD,
	&xSet_CLI_SET_PARITY_BIT_CMD,
	&xSet_CLI_SET_STOP_BIT_CMD,
	&xSet_CLI_SET_MODBUS_PORT_CMD,
#endif
};
#if ENABLE_CHANGE_COUNTRY
#define REGISTER_CLI_COMMANDS MM_ARRAY_COUNT(REGISTER_CLI_COMMAND)
#else
#define REGISTER_CLI_COMMANDS (MM_ARRAY_COUNT(REGISTER_CLI_COMMAND) - 1)
#endif

#if( configGENERATE_RUN_TIME_STATS == 1 )
/* Structure that defines the "run-time-stats" command line command.   This
   generates a table that shows how much run time each task has */
static const CLI_Command_Definition_t xRunTimeStats =
{
	"run-time-stats", /* The command string to type. */
	"\r\nrun-time-stats:\r\n Displays a table showing how much processing time each FreeRTOS task has used\r\n",
	prvRunTimeStatsCommand, /* The function to run. */
	0 /* No parameters are expected. */
};
#endif /* configGENERATE_RUN_TIME_STATS */

#if( configINCLUDE_QUERY_HEAP_COMMAND == 1 )
/* Structure that defines the "query_heap" command line command. */
static const CLI_Command_Definition_t xQueryHeap =
{
	"query-heap",
	"\r\nquery-heap:\r\n Displays the free heap space, and minimum ever free heap space.\r\n",
	prvQueryHeapCommand, /* The function to run. */
	0 /* The user can enter any number of commands. */
};
#endif /* configQUERY_HEAP_COMMAND */

#if configINCLUDE_TRACE_RELATED_CLI_COMMANDS == 1
/* Structure that defines the "trace" command line command.  This takes a single
   parameter, which can be either "start" or "stop". */
static const CLI_Command_Definition_t xStartStopTrace =
{
	"trace",
	"\r\ntrace [start | stop]:\r\n Starts or stops a trace recording for viewing in FreeRTOS+Trace\r\n",
	prvStartStopTraceCommand, /* The function to run. */
	1 /* One parameter is expected.  Valid values are "start" and "stop". */
};
#endif /* configINCLUDE_TRACE_RELATED_CLI_COMMANDS */

/*-----------------------------------------------------------*/

void vRegisterSampleCLICommands( void )
{
	/* Register all the command line commands defined immediately above. */
	for(unsigned int i = 0; i < REGISTER_CLI_COMMANDS; i++) {
		FreeRTOS_CLIRegisterCommand( REGISTER_CLI_COMMAND[i] );
	}

	/* Create scan semaphone */
	scan_finish_semb = mmosal_semb_create("scan_sem");

#if( configGENERATE_RUN_TIME_STATS == 1 )
	{
		FreeRTOS_CLIRegisterCommand( &xRunTimeStats );
	}
#endif

#if( configINCLUDE_QUERY_HEAP_COMMAND == 1 )
	{
		FreeRTOS_CLIRegisterCommand( &xQueryHeap );
	}
#endif

#if( configINCLUDE_TRACE_RELATED_CLI_COMMANDS == 1 )
	{
		FreeRTOS_CLIRegisterCommand( &xStartStopTrace );
	}
#endif
}

/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

static BaseType_t prvTaskStatsCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	const char *const pcHeader = "     State   Priority  Stack    #\r\n************************************************\r\n";
	BaseType_t xSpacePadding;

	/* Remove compile time warnings about unused parameters, and check the
	   write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	   write buffer length is adequate, so does not check for buffer overflows. */
	UNUSED(pcCommandString);
	UNUSED(xWriteBufferLen);
	//( void ) pcCommandString;
	//( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );

	/* Generate a table of task stats. */
	strcpy( pcWriteBuffer, "Task" );
	pcWriteBuffer += strlen( pcWriteBuffer );

	/* Minus three for the null terminator and half the number of characters in
	   "Task" so the column lines up with the centre of the heading. */
	configASSERT( configMAX_TASK_NAME_LEN > 3 );
	for( xSpacePadding = strlen( "Task" ); xSpacePadding < ( configMAX_TASK_NAME_LEN - 3 ); xSpacePadding++ )
	{
		/* Add a space to align columns after the task's name. */
		*pcWriteBuffer = ' ';
		pcWriteBuffer++;

		/* Ensure always terminated. */
		*pcWriteBuffer = 0x00;
	}
	strcpy( pcWriteBuffer, pcHeader );
	vTaskList( pcWriteBuffer + strlen( pcHeader ) );

	/* There is no more data to return after this single string, so return
	   pdFALSE. */
	return pdFALSE;
}
#endif /* ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) ) */

/*-----------------------------------------------------------*/

#if( configINCLUDE_QUERY_HEAP_COMMAND == 1 )

static BaseType_t prvQueryHeapCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	/* Remove compile time warnings about unused parameters, and check the
	   write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	   write buffer length is adequate, so does not check for buffer overflows. */
	UNUSED(pcCommandString);
	UNUSED(xWriteBufferLen);
	//( void ) pcCommandString;
	//( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );

	sprintf( pcWriteBuffer, "Current free heap %d bytes, minimum ever free heap %d bytes\r\n", ( int ) xPortGetFreeHeapSize(), ( int ) xPortGetMinimumEverFreeHeapSize() );

	/* There is no more data to return after this single string, so return
	   pdFALSE. */
	return pdFALSE;
}

#endif /* configINCLUDE_QUERY_HEAP */

/*-----------------------------------------------------------*/

#if( configGENERATE_RUN_TIME_STATS == 1 )

static BaseType_t prvRunTimeStatsCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	const char * const pcHeader = "  Abs Time      % Time\r\n****************************************\r\n";
	BaseType_t xSpacePadding;

	/* Remove compile time warnings about unused parameters, and check the
	   write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	   write buffer length is adequate, so does not check for buffer overflows. */
	UNUSED(pcCommandString);
	UNUSED(xWriteBufferLen);
	//( void ) pcCommandString;
	//( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );

	/* Generate a table of task stats. */
	strcpy( pcWriteBuffer, "Task" );
	pcWriteBuffer += strlen( pcWriteBuffer );

	/* Pad the string "task" with however many bytes necessary to make it the
	   length of a task name.  Minus three for the null terminator and half the
	   number of characters in	"Task" so the column lines up with the centre of
	   the heading. */
	for( xSpacePadding = strlen( "Task" ); xSpacePadding < ( configMAX_TASK_NAME_LEN - 3 ); xSpacePadding++ )
	{
		/* Add a space to align columns after the task's name. */
		*pcWriteBuffer = ' ';
		pcWriteBuffer++;

		/* Ensure always terminated. */
		*pcWriteBuffer = 0x00;
	}

	strcpy( pcWriteBuffer, pcHeader );
	vTaskGetRunTimeStats( pcWriteBuffer + strlen( pcHeader ) );

	/* There is no more data to return after this single string, so return
	   pdFALSE. */
	return pdFALSE;
}

#endif /* configGENERATE_RUN_TIME_STATS */

/*-----------------------------------------------------------*/

#if 0
static BaseType_t prvThreeParameterEchoCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	const char *pcParameter;
	BaseType_t xParameterStringLength, xReturn;
	static UBaseType_t uxParameterNumber = 0;

	/* Remove compile time warnings about unused parameters, and check the
	   write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	   write buffer length is adequate, so does not check for buffer overflows. */
	UNUSED(pcCommandString);
	UNUSED(xWriteBufferLen);
	//( void ) pcCommandString;
	//( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );

	if( uxParameterNumber == 0 )
	{
		/* The first time the function is called after the command has been
		   entered just a header string is returned. */
		sprintf( pcWriteBuffer, "The three parameters were:\r\n" );

		/* Next time the function is called the first parameter will be echoed
		   back. */
		uxParameterNumber = 1U;

		/* There is more data to be returned as no parameters have been echoed
		   back yet. */
		xReturn = pdPASS;
	}
	else
	{
		/* Obtain the parameter string. */
		pcParameter = FreeRTOS_CLIGetParameter
			(
			 pcCommandString,		/* The command string itself. */
			 uxParameterNumber,		/* Return the next parameter. */
			 &xParameterStringLength	/* Store the parameter string length. */
			);

		/* Sanity check something was returned. */
		configASSERT( pcParameter );

		/* Return the parameter string. */
		memset( pcWriteBuffer, 0x00, xWriteBufferLen );
		sprintf( pcWriteBuffer, "%d: ", ( int ) uxParameterNumber );
		strncat( pcWriteBuffer, pcParameter, ( size_t ) xParameterStringLength );
		//strncat( pcWriteBuffer, "\r\n", strlen( "\r\n" ) );
		strcat( pcWriteBuffer, "\r\n" );

		/* If this is the last of the three parameters then there are no more
		   strings to return after this one. */
		if( uxParameterNumber == 3U )
		{
			/* If this is the last of the three parameters then there are no more
			   strings to return after this one. */
			xReturn = pdFALSE;
			uxParameterNumber = 0;
		}
		else
		{
			/* There are more parameters to return after this one. */
			xReturn = pdTRUE;
			uxParameterNumber++;
		}
	}

	return xReturn;
}

/*-----------------------------------------------------------*/

static BaseType_t prvParameterEchoCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	const char *pcParameter;
	BaseType_t xParameterStringLength, xReturn;
	static UBaseType_t uxParameterNumber = 0;

	/* Remove compile time warnings about unused parameters, and check the
	   write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	   write buffer length is adequate, so does not check for buffer overflows. */
	UNUSED(pcCommandString);
	UNUSED(xWriteBufferLen);
	//( void ) pcCommandString;
	//( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );

	if( uxParameterNumber == 0 )
	{
		/* The first time the function is called after the command has been
		   entered just a header string is returned. */
		sprintf( pcWriteBuffer, "The parameters were:\r\n" );

		/* Next time the function is called the first parameter will be echoed
		   back. */
		uxParameterNumber = 1U;

		/* There is more data to be returned as no parameters have been echoed
		   back yet. */
		xReturn = pdPASS;
	}
	else
	{
		/* Obtain the parameter string. */
		pcParameter = FreeRTOS_CLIGetParameter
			(
			 pcCommandString,		/* The command string itself. */
			 uxParameterNumber,		/* Return the next parameter. */
			 &xParameterStringLength	/* Store the parameter string length. */
			);

		if( pcParameter != NULL )
		{
			/* Return the parameter string. */
			memset( pcWriteBuffer, 0x00, xWriteBufferLen );
			sprintf( pcWriteBuffer, "%d: ", ( int ) uxParameterNumber );
			strncat( pcWriteBuffer, ( char * ) pcParameter, ( size_t ) xParameterStringLength );
			//strncat( pcWriteBuffer, "\r\n", strlen( "\r\n" ) );
			strcat( pcWriteBuffer, "\r\n" );

			/* There might be more parameters to return after this one. */
			xReturn = pdTRUE;
			uxParameterNumber++;
		}
		else
		{
			/* No more parameters were found.  Make sure the write buffer does
			   not contain a valid string. */
			pcWriteBuffer[ 0 ] = 0x00;

			/* No more data to return. */
			xReturn = pdFALSE;

			/* Start over the next time this command is executed. */
			uxParameterNumber = 0;
		}
	}

	return xReturn;
}
#endif

/*-----------------------------------------------------------*/

#if configINCLUDE_TRACE_RELATED_CLI_COMMANDS == 1
static BaseType_t prvStartStopTraceCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	const char *pcParameter;
	BaseType_t lParameterStringLength;

	/* Remove compile time warnings about unused parameters, and check the
	   write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	   write buffer length is adequate, so does not check for buffer overflows. */
	UNUSED(pcCommandString);
	UNUSED(xWriteBufferLen);
	//( void ) pcCommandString;
	//( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );

	/* Obtain the parameter string. */
	pcParameter = FreeRTOS_CLIGetParameter
		(
		 pcCommandString,		/* The command string itself. */
		 1,						/* Return the first parameter. */
		 &lParameterStringLength	/* Store the parameter string length. */
		);

	/* Sanity check something was returned. */
	configASSERT( pcParameter );

	/* There are only two valid parameter values. */
	if( strncmp( pcParameter, "start", strlen( "start" ) ) == 0 )
	{
		/* Start or restart the trace. */
		vTraceStop();
		vTraceClear();
		vTraceStart();

		sprintf( pcWriteBuffer, "Trace recording (re)started.\r\n" );
	}
	else if( strncmp( pcParameter, "stop", strlen( "stop" ) ) == 0 )
	{
		/* End the trace, if one is running. */
		vTraceStop();
		sprintf( pcWriteBuffer, "Stopping trace recording.\r\n" );
	}
	else
	{
		sprintf( pcWriteBuffer, "Valid parameters are 'start' and 'stop'.\r\n" );
	}

	/* There is no more data to return after this single string, so return
	   pdFALSE. */
	return pdFALSE;
}

#endif /* configINCLUDE_TRACE_RELATED_CLI_COMMANDS */

/*-----------------------------------------------------------*/

static BaseType_t prvReboot_CMD( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	const char *pcParameter;
	BaseType_t xParameterStringLength, xReturn;
	static UBaseType_t uxParameterNumber = 0U;

	UNUSED(xWriteBufferLen);
	UNUSED(pcCommandString);
	UNUSED(pcParameter);
	UNUSED(xParameterStringLength);

	configASSERT( pcWriteBuffer );

	if( uxParameterNumber == 0 ) {
		uxParameterNumber = 0U;
		xReturn = pdPASS;

#if 0
		Jump_to_address(FLASH_FW_ADDR);
#else

#ifdef STM_USBD_ACM_ENABLE
		USBD_Stop(&hUsbDeviceFS);
#else
#error "STM_USBD_ACM_ENABLE no define."
#endif
		mmosal_task_sleep(1000);

		mmhal_reset();
#endif
	}
	else {
		xReturn = pdPASS;
		uxParameterNumber = 0U;
	}

	return xReturn;
}

#if 0
/* @brief  Jumps to the specified target address in the memory.
 * @param  address: Pointer to the memory address to which the jump should occur.
 * @return None
 * @note   This function changes the execution flow of the program by setting
 *         the program counter to the specified address. Ensure the target address
 *         is valid and contains executable code to prevent undefined behavior.
 */
static void Jump_to_address(uint32_t address)
{
	//__DBG("address 0x%x\n", address);
	//JumpAddress = *(__IO uint32_t*) (address + BOOT_FLASH_SHIFT);
	JumpAddress = *(__IO uint32_t*) (address);
	Jump_To = (pFunction) JumpAddress;
	//__DBG("JmupAddr 0x%x\n", JumpAddress);

	/* Initialize user application's Stack Pointer */
	__set_MSP(*(__IO uint32_t*) address);

	mmosal_task_sleep(20);

	Jump_To();

	/* do nothing */
	while(1) {
		// You should never stay here
	}
}
#endif

/*-----------------------------------------------------------*/

const char *get_conf_key_of_dev(CLI_CMD_E cmd, const char* dev)
{
	static char key_buffer[BUFFER_SIZE];
	memset(key_buffer, 0x00, BUFFER_SIZE);

	strncat(key_buffer, CLI_CMD_MAP[cmd].conf_key, strlen(CLI_CMD_MAP[cmd].conf_key));
	strncat(key_buffer, dev, 1);

	return key_buffer;
}

int config_write_str_of_dev(CLI_CMD_E cmd, const char *value, size_t len, const char* dev)
{
	// We need enough buffer for passphare(63 bytes) + '\0'(1 byte)
	static char buf[BUFFER_SIZE_LONG];
	memset(buf, 0x00, BUFFER_SIZE_LONG);
	if (len >= BUFFER_SIZE_LONG)
		return MMCONFIG_ERR_OUT_OF_BOUNDS;

	strncpy(buf, value, len);
	int ret = mmconfig_write_string(get_conf_key_of_dev(cmd, dev), buf);

	return ret;
}

int config_read_str_of_dev(CLI_CMD_E cmd, char *buffer, int bufsize, const char* dev)
{
	int ret = mmconfig_read_string(get_conf_key_of_dev(cmd, dev), buffer, bufsize);

	return ret;
}

int config_write_str(CLI_CMD_E cmd, const char *value, size_t len)
{
	// We need enough buffer for passphare(63 bytes) + '\0'(1 byte)
	static char buf[BUFFER_SIZE_LONG];
	memset(buf, 0x00, BUFFER_SIZE_LONG);
	if (len >= BUFFER_SIZE_LONG)
		return MMCONFIG_ERR_OUT_OF_BOUNDS;

	strncpy(buf, value, len);

	int ret = mmconfig_write_string(CLI_CMD_MAP[cmd].conf_key, buf);

	return ret;
}

int config_read_str(CLI_CMD_E cmd, char *buffer, int bufsize)
{
	int ret = mmconfig_read_string(CLI_CMD_MAP[cmd].conf_key, buffer, bufsize);

	return ret;
}

int config_write_bool(CLI_CMD_E cmd, bool value)
{
	int ret = mmconfig_write_bool(CLI_CMD_MAP[cmd].conf_key, value);

	return ret;
}


int config_read_bool(CLI_CMD_E cmd, bool *value)
{
	int ret = mmconfig_read_bool(CLI_CMD_MAP[cmd].conf_key, value);

	return ret;
}

/*-----------------------------------------------------------*/

int isValidSSID(const char *ssid, size_t len)
{
	/* Check length, WPA3 SSID max length = 31 + '\0' */
	if (len == 0 || len >= MAX_SSID_LEN) {
		return 0; /* invalid */
	}

	/* Check character */
	for (size_t i = 0; i < len; i++) {
		if (ssid[i] < 0x20 || ssid[i] > 0x7E) {
			return 0; /* not printable character */
		}
	}

	return 1; /* valid */
}


int isValidPassword(const char *password, size_t len)
{
	/* Check length, WPA3 password should be 8 ~ 63 + '\0' characters. */
	if (len < MIN_PASSWORD_LEN || len >= MAX_PASSWORD_LEN) {
		return 0; /* invalid */
	}

	/* Check character */
	for (size_t i = 0; i < len; i++) {
		if (password[i] < 0x20 || password[i] > 0x7E) {
			return 0; /* not printable character */
		}
	}

	return 1; /* valid */
}


int isValidSecurity(const char *security, size_t len)
{
	if (len == 3 && strncmp("sae", security, len) == 0)
		return 1;
	if (len == 3 && strncmp("owe", security, len) == 0)
		return 1;
	if (len == 4 && strncmp("open", security, len) == 0)
		return 1;

	return 0; /* invalid */
}



int isValidCountry(const char *security, size_t len)
{
	if (len != 2)
		return 0;

	for (size_t i = 0; i < len; i ++) {
		if (security[i] < 'A' || security[i] > 'Z') {
			return 0;
		}
	}

	return 1; /*  valid */
}


int isValidBool(const char *dhcp, size_t len)
{
	if (len != 1)
		return 0;

	if (strncmp(dhcp, "0", 1) == 0)
		return 1;
	else if (strncmp(dhcp, "1", 1) == 0)
		return 1;

	return 0;
}


int isValidIPv4(const char *addr, size_t len)
{
	if (len > 15 || len < 7) {
		return 0;
	}

	char addrCopy[MAX_ADDR_STR_LEN] = {0};
	strncpy(addrCopy, addr, len);
	const char *delim = ".";
	char *token = strtok(addrCopy, delim);
	int segment = 0;

	while (token != NULL) {
		/* Check is it number */
		for (size_t i = 0; i < strlen(token); i++) {
			if (!isdigit((unsigned char)token[i])) {
				printf("Error: '%s' contains non-numeric character.\n", token);
				return 0;
			}
		}

		/* Change to int for check range */
		int num = atoi(token);
		if (num < 0 || num > 255) {
			printf("Error: '%s' is out of range (0-255).\n", token);
			return 0;
		}

		segment++;
		token = strtok(NULL, delim); /*  Check next segment*/
	}

	/* Check is it exactly 4 segments */
	if (segment != 4) {
		printf("Error: Address '%s' has no exactly 4 parts.\n", addr);
		return 0;
	}

	return 1;
}

int isValidDevice(const char *device, size_t len)
{
	char buf[BUFFER_SIZE] = {0};
	int dev;
	if (len >= BUFFER_SIZE)
		return 0;
	
	strncpy(buf, device, len);
	dev = atoi(buf);

	return len == 1 && dev > 0 && dev <= MODBUS_NUM ? 1 : 0;
}

int isValidBaudrate(const char *baudrate, size_t len)
{
	static const char *availBaudrate[] = {"1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"};
	char buf[BUFFER_SIZE] = {0};
	if (len >= BUFFER_SIZE)
		return 0;
	
	strncpy(buf, baudrate, len);
	buf[BUFFER_SIZE-1] = '\0';
	for (int i = 0; i < (int)MM_ARRAY_COUNT(availBaudrate); i++) {
		if (strcmp(buf, availBaudrate[i]) == 0) {
			return 1;
		}
	}

	return 0;
}

int isValidParityBit(const char *parity_bit, size_t len)
{
	static const char *availParityBit[] = {"none", "even", "odd"};
	char buf[BUFFER_SIZE] = {0};
	if (len >= BUFFER_SIZE)
		return 0;

	strncpy(buf, parity_bit, len);
	for (int i = 0; i < (int)MM_ARRAY_COUNT(availParityBit); i++) {
		if (strcmp(buf, availParityBit[i]) == 0) {
			return 1;
		}
	}

	return 0;
}
int isValidDataBit(const char *data_bit, size_t len)
{
	static const char *availDataBit[] = {"7", "8", "9"};
	char buf[BUFFER_SIZE] = {0};
	if (len >= BUFFER_SIZE)
		return 0;

	strncpy(buf, data_bit, len);
	for (int i = 0; i < (int)MM_ARRAY_COUNT(availDataBit); i++) {
		if (strcmp(buf, availDataBit[i]) == 0) {
			return 1;
		}
	}

	return 0;
}
int isValidStopBit(const char *stop_bit, size_t len)
{
	static const char *availStopBit[] = {"1", "1.5", "2"};
	char buf[BUFFER_SIZE] = {0};
	if (len >= BUFFER_SIZE)
		return 0;

	strncpy(buf, stop_bit, len);
	for (int i = 0; i < (int)MM_ARRAY_COUNT(availStopBit); i++) {
		if (strcmp(buf, availStopBit[i]) == 0) {
			return 1;
		}
	}

	return 0;
}
int isValidModbusPort(const char *modbus_port, size_t len)
{
	char buf[BUFFER_SIZE] = {0};
	int port;
	if (len >= BUFFER_SIZE)
		return 0;

	strncpy(buf, modbus_port, len);
	sprintf(buf, "%d", atoi(buf));
	port = atoi(buf);

	return strlen(buf) == len && port > 0 && port <= 65535 ? 1 : 0;
}

/*-----------------------------------------------------------*/

void vCheckCommandLineDefaultValues()
{
	// Check and set default SSID
	char ssid[MAX_SSID_LEN] = {0};
	if (config_read_str(CLI_SET_SSID, ssid, sizeof(ssid)) <= 0) {
		config_write_str(CLI_SET_SSID, DEFAULT_SSID, strlen(DEFAULT_SSID));
	}

	// Check and set default password
	char password[MAX_PASSWORD_LEN] = {0};
	if (config_read_str(CLI_SET_PASSWORD, password, sizeof(password)) <= 0) {
		config_write_str(CLI_SET_PASSWORD, DEFAULT_PASSWORD, strlen(DEFAULT_PASSWORD));
	}

	// Check and set default security
	char security[BUFFER_SIZE_SHORT] = {0};
	if (config_read_str(CLI_SET_SECURITY, security, sizeof(security)) <= 0) {
		config_write_str(CLI_SET_SECURITY, DEFAULT_SECURITY, strlen(DEFAULT_SECURITY));
	}

#if ENABLE_CHANGE_COUNTRY
	// Check and set default country
	char country[BUFFER_SIZE_SHORT] = {0};
	if (config_read_str(CLI_SET_COUNTRY, country, sizeof(country)) <= 0) {
		config_write_str(CLI_SET_COUNTRY, DEFAULT_COUNTRY, strlen(DEFAULT_COUNTRY));
	}
#endif

	// Check and set default DHCP (false)
	bool dhcp = false;
	if (config_read_bool(CLI_SET_DHCP, &dhcp) != MMCONFIG_OK ) {
		config_write_bool(CLI_SET_DHCP, DEFAULT_DHCP);
	}

	// Check and set default IP
	char ip[MAX_ADDR_STR_LEN] = {0};
	if (config_read_str(CLI_SET_IP, ip, sizeof(ip)) <= 0) {
		config_write_str(CLI_SET_IP, DEFAULT_IPADDR, strlen(DEFAULT_IPADDR));
	}

	// Check and set default netmask
	char netmask[MAX_ADDR_STR_LEN] = {0};
	if (config_read_str(CLI_SET_NETMASK, netmask, sizeof(netmask)) <= 0) {
		config_write_str(CLI_SET_NETMASK, DEFAULT_NETMASK, strlen(DEFAULT_NETMASK));
	}

	// Check and set default gateway
	char gateway[MAX_ADDR_STR_LEN] = {0};
	if (config_read_str(CLI_SET_GATEWAY, gateway, sizeof(gateway)) <= 0) {
		config_write_str(CLI_SET_GATEWAY, DEFAULT_GATEWAY, strlen(DEFAULT_GATEWAY));
	}

	// Check and set default powersave (disabled)
	char powersave[BUFFER_SIZE] = {0};
	if (config_read_str(CLI_SET_POWERSAVE, powersave, sizeof(powersave)) <= 0) {
		config_write_str(CLI_SET_POWERSAVE, DEFAULT_POWERSAVE, strlen(DEFAULT_POWERSAVE));
	}

#if MODBUS_NUM != 0
	// Check and set default values for serial/modbus
	char dev[8] = {0}; // Large enough for any uint8_t value as string
	char buf[BUFFER_SIZE] = {0};
	char writebuf[BUFFER_SIZE] = {0};
	for (uint8_t i = 1; i <= MODBUS_NUM; i++)
	{
		snprintf(dev, sizeof(dev), "%u", i);
		if (config_read_str_of_dev(CLI_SET_BAUDRATE, buf, sizeof(buf), dev) <= 0) {
			config_write_str_of_dev(CLI_SET_BAUDRATE, DEFAULT_BAUDRATE, strlen(DEFAULT_BAUDRATE), dev);
		}
		if (config_read_str_of_dev(CLI_SET_DATA_BIT, buf, sizeof(buf), dev) <= 0) {
			config_write_str_of_dev(CLI_SET_DATA_BIT, DEFAULT_DATA_BIT, strlen(DEFAULT_DATA_BIT), dev);
		}
		if (config_read_str_of_dev(CLI_SET_PARITY_BIT, buf, sizeof(buf), dev) <= 0) {
			config_write_str_of_dev(CLI_SET_PARITY_BIT, DEFAULT_PARITY_BIT, strlen(DEFAULT_PARITY_BIT), dev);
		}
		if (config_read_str_of_dev(CLI_SET_STOP_BIT, buf, sizeof(buf), dev) <= 0) {
			config_write_str_of_dev(CLI_SET_STOP_BIT, DEFAULT_STOP_BIT, strlen(DEFAULT_STOP_BIT), dev);
		}
		if (config_read_str_of_dev(CLI_SET_MODBUS_PORT, buf, sizeof(buf), dev) <= 0) {
			snprintf(writebuf, sizeof(writebuf), "%u", atoi(DEFAULT_MODBUS_PORT) + i - 1);
			config_write_str_of_dev(CLI_SET_MODBUS_PORT, writebuf, strlen(writebuf), dev);
		}
	}
#endif
}

/*-----------------------------------------------------------*/

void vResetDefaultSettings()
{
	// Reset all settings to default values
	config_write_str(CLI_SET_SSID, DEFAULT_SSID, strlen(DEFAULT_SSID));
	config_write_str(CLI_SET_PASSWORD, DEFAULT_PASSWORD, strlen(DEFAULT_PASSWORD));
	config_write_str(CLI_SET_SECURITY, DEFAULT_SECURITY, strlen(DEFAULT_SECURITY));
#if ENABLE_CHANGE_COUNTRY
	config_write_str(CLI_SET_COUNTRY, DEFAULT_COUNTRY, strlen(DEFAULT_COUNTRY));
#endif
	config_write_bool(CLI_SET_DHCP, DEFAULT_DHCP);
	config_write_str(CLI_SET_IP, DEFAULT_IPADDR, strlen(DEFAULT_IPADDR));
	config_write_str(CLI_SET_NETMASK, DEFAULT_NETMASK, strlen(DEFAULT_NETMASK));
	config_write_str(CLI_SET_GATEWAY, DEFAULT_GATEWAY, strlen(DEFAULT_GATEWAY));
	config_write_str(CLI_SET_POWERSAVE, DEFAULT_POWERSAVE, strlen(DEFAULT_POWERSAVE));

#if MODBUS_NUM != 0
	// Reset all settings for serial/modbus to default values
	char dev[4] = {0};
	char buf[BUFFER_SIZE] = {0};
	for (uint8_t i = 1; i <= MODBUS_NUM; i++)
	{
		snprintf(dev, sizeof(dev), "%u", i);
		config_write_str_of_dev(CLI_SET_BAUDRATE, DEFAULT_BAUDRATE, strlen(DEFAULT_BAUDRATE), dev);
		config_write_str_of_dev(CLI_SET_DATA_BIT, DEFAULT_DATA_BIT, strlen(DEFAULT_DATA_BIT), dev);
		config_write_str_of_dev(CLI_SET_PARITY_BIT, DEFAULT_PARITY_BIT, strlen(DEFAULT_PARITY_BIT), dev);
		config_write_str_of_dev(CLI_SET_STOP_BIT, DEFAULT_STOP_BIT, strlen(DEFAULT_STOP_BIT), dev);
		snprintf(buf, sizeof(buf), "%d", atoi(DEFAULT_MODBUS_PORT) + i - 1);
		config_write_str_of_dev(CLI_SET_MODBUS_PORT, buf, strlen(buf), dev);
	}
#endif
}

/*-----------------------------------------------------------*/

static BaseType_t prvInfo_CMD( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	const char *pcParameter;
	BaseType_t xParameterStringLength, xReturn = pdFALSE;
	static UBaseType_t uxParameterNumber = 0U;
#if MODBUS_NUM != 0
	char dev[4] = {0};
	static uint8_t i = 1;
#endif
	uint8_t mac_addr[MMWLAN_MAC_ADDR_LEN] = { 0 };
    MMOSAL_ASSERT(mmwlan_get_mac_addr(mac_addr) == MMWLAN_SUCCESS);

	UNUSED(xWriteBufferLen);
	UNUSED(pcCommandString);
	UNUSED(pcParameter);
	UNUSED(xParameterStringLength);

	configASSERT( pcWriteBuffer );
	CLI_INFO_t cli_info;
#if MODBUS_NUM != 0
	CLI_SERIAL_INFO_t cli_serial_info;
#endif

	struct mmipal_ip_config ip_config = MMIPAL_IP_CONFIG_DEFAULT;
	enum mmipal_status status;

	switch (uxParameterNumber) {

		case 0U:
			status = mmipal_get_ip_config(&ip_config);
			if (status != MMIPAL_SUCCESS)
			{
				printf("Failed to retrieve IP config\n");
			}

			// Print Information about Wi-Fi
			config_read_str(CLI_SET_SSID, cli_info.ssid, sizeof(cli_info.ssid));
			config_read_str(CLI_SET_PASSWORD, cli_info.password, sizeof(cli_info.password));
			config_read_str(CLI_SET_SECURITY, cli_info.security, sizeof(cli_info.security));
#if ENABLE_CHANGE_COUNTRY
			config_read_str(CLI_SET_COUNTRY, cli_info.country, sizeof(cli_info.country));
#else
			char country[4];
			mmconfig_read_string("wlan.country_code", country, sizeof(country));
#endif
			if (ip_config.mode == MMIPAL_STATIC || ip_config.mode == MMIPAL_DHCP) {
				if (ip_config.mode == MMIPAL_DHCP)
					cli_info.dhcp = true;
				else
					cli_info.dhcp = false;

				strncpy(cli_info.ipaddr, ip_config.ip_addr, sizeof(cli_info.ipaddr));
				strncpy(cli_info.netmask, ip_config.netmask, sizeof(cli_info.netmask));
				strncpy(cli_info.gateway, ip_config.gateway_addr, sizeof(cli_info.gateway));
				//printf("ip: %s, net: %s, gate: %s\n", ip_config.ip_addr, ip_config.netmask, ip_config.gateway_addr);
				//tud_cdc_write_str(ip_config.ip_addr);
				//tud_cdc_write_str(ip_config.netmask);
				//tud_cdc_write_str(ip_config.gateway_addr);
			} 
			else {
				config_read_bool(CLI_SET_DHCP, &cli_info.dhcp);
				config_read_str(CLI_SET_IP, cli_info.ipaddr, sizeof(cli_info.ipaddr));
				config_read_str(CLI_SET_NETMASK, cli_info.netmask, sizeof(cli_info.netmask));
				config_read_str(CLI_SET_GATEWAY, cli_info.gateway, sizeof(cli_info.gateway));
			}
			config_read_str(CLI_SET_POWERSAVE, cli_info.powersave, sizeof(cli_info.powersave));

			sprintf(pcWriteBuffer,
				"\r\n"
				"[Device information]\r\n"
				"-----------------------------------------\r\n"
#ifdef PLATFORM_NAME
				" Device\t\t : %s\r\n"
#endif
				" Firmware\t : " RELEASE_VERSION "\r\n"
				" SSID\t\t : %s\r\n"
				" Password\t : %s\r\n"
				" Security\t : %s\r\n"
				" Country\t : %s\r\n"
				" DHCP\t\t : %s\r\n"
				" IP\t\t : %s\r\n"
				" Netmask\t : %s\r\n"
				" Gateway\t : %s\r\n"
				" MAC\t\t : %02X:%02X:%02X:%02X:%02X:%02X\r\n"
				" Power save\t : %s\r\n",
#ifdef PLATFORM_NAME
				PLATFORM_NAME,
#endif
				cli_info.ssid,
				cli_info.password,
				cli_info.security,
#if ENABLE_CHANGE_COUNTRY
				cli_info.country,
#else
				country,
#endif
				(cli_info.dhcp == true ? "ON" : "OFF"),
				cli_info.ipaddr,
				cli_info.netmask,
				cli_info.gateway,
				mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5],
				cli_info.powersave);

			uxParameterNumber = 1U;
			xReturn = pdPASS;
			break;
		case 1U:
#if MODBUS_NUM == 0
			memset(pcWriteBuffer, 0, xWriteBufferLen);
			uxParameterNumber = 0U;
			xReturn = pdFALSE;
			break;
#else
			// Print Information about Serial/Modbus
			snprintf(dev, sizeof(dev), "%u", i);
			config_read_str_of_dev(CLI_SET_BAUDRATE, cli_serial_info.baudrate, sizeof(cli_serial_info.baudrate), dev);
			config_read_str_of_dev(CLI_SET_DATA_BIT, cli_serial_info.data_bit, sizeof(cli_serial_info.data_bit), dev);
			config_read_str_of_dev(CLI_SET_PARITY_BIT, cli_serial_info.parity_bit, sizeof(cli_serial_info.parity_bit), dev);
			config_read_str_of_dev(CLI_SET_STOP_BIT, cli_serial_info.stop_bit, sizeof(cli_serial_info.stop_bit), dev);
			config_read_str_of_dev(CLI_SET_MODBUS_PORT, cli_serial_info.modbus_port, sizeof(cli_serial_info.modbus_port), dev);

			sprintf(pcWriteBuffer, \
				"\r\n" \
				"[Serial %s information]\r\n" \
				"-----------------------------------------\r\n" \
				" Baudrate\t : %s\r\n" \
				" Data bit\t : %s\r\n" \
				" Parity bit\t : %s\r\n" \
				" Stop bit\t : %s\r\n" \
				" Modbus port\t : %s\r\n", \
				dev, cli_serial_info.baudrate, cli_serial_info.data_bit, cli_serial_info.parity_bit, cli_serial_info.stop_bit, cli_serial_info.modbus_port);

			i++;
			if (i > MODBUS_NUM) {
				uxParameterNumber = 0U;
				i = 1;
				xReturn = pdFALSE;
			}
			else {
				xReturn = pdPASS;
			}
			break;
#endif
		default:
			//print error message
			sprintf(pcWriteBuffer, "Error: Error Occurs When Gatheriing Infomation.\r\n");
			break;
	}
	return xReturn;
}

static BaseType_t prvReset_CMD( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	const char *pcParameter;
	BaseType_t xParameterStringLength, xReturn = pdFALSE;

	UNUSED(xWriteBufferLen);
	UNUSED(pcCommandString);
	UNUSED(pcParameter);
	UNUSED(xParameterStringLength);
	configASSERT( pcWriteBuffer );

	sprintf( pcWriteBuffer, "Reset Configurations.\r\n" );
	//Reset all settings to default values
	vResetDefaultSettings();

	/* There is no more data to return after this single string, so return
	   pdFALSE. */
	return xReturn;
}

/*-----------------------------------------------------------*/

/*
 * If ASNI_ESCAPE_ENABLED is non-zero (the default) then ANSI escape characters will be used to
 *  format the log output.
 */
#if !(defined(ASNI_ESCAPE_ENABLED) && ASNI_ESCAPE_ENABLED == 0)
/** ANSI escape sequence for bold text. */
#define ANSI_BOLD  "\x1b[1m"
/** ANSI escape sequence to reset font. */
#define ANSI_RESET "\x1b[0m"
#else
/** ANSI escape sequence for bold text (disabled so no-op). */
#define ANSI_BOLD  ""
/** ANSI escape sequence to reset font (disabled so no-op). */
#define ANSI_RESET ""
#endif

/** Length of string representation of a MAC address (i.e., "XX:XX:XX:XX:XX:XX")
 * including null terminator. */
#define MAC_ADDR_STR_LEN    (18)

#if 1
/** Number of results found. */
static int num_scan_results;

/**
 * Scan rx callback.
 *
 * @param result        Pointer to the scan result.
 * @param arg           Opaque argument.
 */
static void scan_rx_callback(const struct mmwlan_scan_result *result, void *arg)
{
    (void)(arg);
    char bssid_str[MAC_ADDR_STR_LEN];
    char ssid_str[MMWLAN_SSID_MAXLEN];
	char output_str[BUFFER_SIZE_LONG] = {0};
    int ret;
    struct mm_rsn_information rsn_info;

    num_scan_results++;
    snprintf(bssid_str, MAC_ADDR_STR_LEN, "%02x:%02x:%02x:%02x:%02x:%02x",
             result->bssid[0], result->bssid[1], result->bssid[2], result->bssid[3],
             result->bssid[4], result->bssid[5]);
    snprintf(ssid_str, (result->ssid_len+1), "%s", result->ssid);

#if 0
    printf(ANSI_BOLD "%2d. %s" ANSI_RESET "\n", num_scan_results, ssid_str);
    printf("    Operating BW: %u MHz\n",  result->op_bw_mhz);
    printf("    BSSID: %s\n", bssid_str);
    printf("    RSSI: %3d\n", result->rssi);
    printf("    Beacon Interval(TUs): %u\n", result->beacon_interval);
    printf("    Capability Info: 0x%04x\n", result->capability_info);
#else
    snprintf(output_str, sizeof(output_str), ANSI_BOLD "%2d. %s" ANSI_RESET "\r\n", num_scan_results, ssid_str);
	vSerialPutString(xPort, (signed char *)output_str, strlen(output_str));
    snprintf(output_str, sizeof(output_str), "    Operating BW: %u MHz\r\n",  result->op_bw_mhz);
	vSerialPutString(xPort, (signed char *)output_str, strlen(output_str));
    snprintf(output_str, sizeof(output_str), "    BSSID: %s\r\n", bssid_str);
	vSerialPutString(xPort, (signed char *)output_str, strlen(output_str));
    snprintf(output_str, sizeof(output_str), "    RSSI: %3d\r\n", result->rssi);
	vSerialPutString(xPort, (signed char *)output_str, strlen(output_str));
    snprintf(output_str, sizeof(output_str), "    Beacon Interval(TUs): %u\r\n", result->beacon_interval);
	vSerialPutString(xPort, (signed char *)output_str, strlen(output_str));
    snprintf(output_str, sizeof(output_str), "    Capability Info: 0x%04x\r\n", result->capability_info);
	vSerialPutString(xPort, (signed char *)output_str, strlen(output_str));
#endif

    ret = mm_parse_rsn_information(result->ies, result->ies_len, &rsn_info);
    if (ret == 0)
    {
        unsigned ii;
        snprintf(output_str, sizeof(output_str), "    Security:");
		vSerialPutString(xPort, (signed char *)output_str, strlen(output_str));

        for (ii = 0; ii < rsn_info.num_akm_suites; ii++)
        {
            snprintf(output_str, sizeof(output_str), " %s", mm_akm_suite_to_string(rsn_info.akm_suites[ii]));
			vSerialPutString(xPort, (signed char *)output_str, strlen(output_str));
        }
        //printf("\n");
		vSerialPutString(xPort, (signed char *)"\r\n", strlen("\r\n"));
    }
    else if (ret == -1 || rsn_info.num_akm_suites == 0)
    {
        snprintf(output_str, sizeof(output_str), "    Security: None\r\n");
		vSerialPutString(xPort, (signed char *)output_str, strlen(output_str));
    }
    else
    {
        snprintf(output_str, sizeof(output_str), "    Invalid RSN IE in probe response\r\n");
		vSerialPutString(xPort, (signed char *)output_str, strlen(output_str));
    }
}

/**
 * Scan complete callback.
 *
 * @param state         Scan complete status.
 * @param arg           Opaque argument.
 */
static void scan_complete_callback(enum mmwlan_scan_state state, void *arg)
{
    (void)(state);
    (void)(arg);
	//printf("Scanning completed.\n");
	vSerialPutString(xPort, (signed char *)"Scanning completed.\r\n", strlen("Scanning completed.\r\n"));
	mmosal_semb_give(scan_finish_semb);
}
#endif

static BaseType_t prvScan_CMD( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	const char *pcParameter;
	BaseType_t xParameterStringLength, xReturn = pdFALSE;
	enum mmwlan_status status;
	char output_str[BUFFER_SIZE_LONG] = {0};

	UNUSED(pcCommandString);
	UNUSED(pcParameter);
	UNUSED(xParameterStringLength);
	configASSERT( pcWriteBuffer );

	num_scan_results = 0;

	const struct mmwlan_s1g_channel_list* channel_list = load_channel_list();

	sprintf(output_str, "\r\n[Scan]\r\n");
	vSerialPutString(xPort, (signed char *)output_str, strlen(output_str));
	sprintf(output_str, "Scan started on \"%s\" channels, Waiting for results...\r\n", channel_list->country_code);
	vSerialPutString(xPort, (signed char *)output_str, strlen(output_str));

	struct mmwlan_scan_req scan_req = MMWLAN_SCAN_REQ_INIT;
    scan_req.scan_rx_cb = scan_rx_callback;
    scan_req.scan_complete_cb = scan_complete_callback;
    status = mmwlan_scan_request(&scan_req);
    MMOSAL_ASSERT(status == MMWLAN_SUCCESS);

	memset(pcWriteBuffer, 0x0, xWriteBufferLen);
	mmosal_semb_wait(scan_finish_semb, UINT32_MAX);

	return xReturn;
}

/*-----------------------------------------------------------*/


static BaseType_t prvGet_PARM_CMD( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString ) {
	BaseType_t xReturn = pdFALSE; // We don't need to come back again.
	static UBaseType_t uxParameterNumber = 1U;
	const char *pcParams[PARAM_MAX_COUNT];
	BaseType_t xParamStrLen[PARAM_MAX_COUNT];
	int index = -1;

	if( uxParameterNumber == 1U ) {
		/* Get function name */
		pcParams[PARAM_COMMAND] = FreeRTOS_CLIGetParameter
			(
			 pcCommandString,		/* The command string itself. */
			 uxParameterNumber,			/* Return the next parameter. */
			 &xParamStrLen[PARAM_COMMAND]		/* Store the parameter string length. */
			);

		index = get_CLI_CMD_INDEX_by_string(pcParams[PARAM_COMMAND], xParamStrLen[PARAM_COMMAND]);
		if (index < 0 || index >= CLI_CMD_COUNT) {
			sprintf(pcWriteBuffer, "Error: Invalid command.\r\n");
			return pdFALSE;
		}

		memset( pcWriteBuffer, 0x00, xWriteBufferLen );

		switch (index) {
			case CLI_GET_RSSI:
				sprintf(pcWriteBuffer, "\r\n[Get RSSI]\r\n");
				char val[BUFFER_SIZE_SHORT] = {0};
				//RSSI will not update if sta is disconnected, so check state first.
				switch (mmwlan_get_sta_state()) {
					case MMWLAN_STA_DISABLED:
					case MMWLAN_STA_CONNECTING:
						strcat(pcWriteBuffer, "Not connected.\r\n");
						break;
					case MMWLAN_STA_CONNECTED:
						sprintf(val, "RSSI: %ld dBm", mmwlan_get_rssi());
						strcat(pcWriteBuffer, val);
						break;
					default:
						strcat(pcWriteBuffer, "Error: Invalid device state.\r\n");
						break;
				}				
				break;
			default:
				sprintf(pcWriteBuffer, "Error: Invalid command.\r\n");
				break;
		}
	}
	
	return xReturn;
}

/*-----------------------------------------------------------*/

static BaseType_t prvSet_PARM_CMD( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	int pcParamCounts = 0;
	const char *pcParams[PARAM_MAX_COUNT];
	BaseType_t xParamStrLen[PARAM_MAX_COUNT];
	static UBaseType_t uxFirstParam = 1U;
	UBaseType_t uxParamNumber = 2U;

	BaseType_t xReturn = pdFALSE; // This callback doesn't need to come back again.
	int index = -1;

	configASSERT( pcWriteBuffer );

	if( uxFirstParam == 1U ) {
		/* Get function name */
		pcParams[PARAM_COMMAND] = FreeRTOS_CLIGetParameter
			(
			 pcCommandString,		/* The command string itself. */
			 uxFirstParam,			/* Return the next parameter. */
			 &xParamStrLen[PARAM_COMMAND]		/* Store the parameter string length. */
			);

		index = get_CLI_CMD_INDEX_by_string(pcParams[PARAM_COMMAND], xParamStrLen[PARAM_COMMAND]);
		if (index < 0 || index >= CLI_CMD_COUNT) {
			sprintf(pcWriteBuffer, "Error: Invalid command.\r\n");
			return pdFALSE;
		}
		pcParamCounts = CLI_CMD_MAP[index].param_counts;

		for (int i = 1; i < pcParamCounts; i++) {
			/* Get parameter count */
			pcParams[i] = FreeRTOS_CLIGetParameter
			(
			 pcCommandString,	/* The command string itself. */
			 uxParamNumber,		/* Return the next parameter. */
				 &xParamStrLen[i]		/* Store the parameter string length. */
			);

			if (pcParams[i] == NULL) {
				sprintf(pcWriteBuffer, "Error: Invalid command.\r\n");
				return pdFALSE;
			}
			uxParamNumber++;
		}

		memset( pcWriteBuffer, 0x00, xWriteBufferLen );

		switch (index) {

			case CLI_SET_SSID:
				sprintf(pcWriteBuffer, "\r\n[Set SSID]\r\n");

				/* Valid SSID format */
				if (isValidSSID(pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1])) {
					char ssid[MMWLAN_SSID_MAXLEN];
					int ret = config_read_str(CLI_SET_SSID, ssid, sizeof(ssid));

					if (ret > 0) {
						strcat(pcWriteBuffer, " Current SSID\t : \"");
						strncat(pcWriteBuffer, ssid, ret);
						strcat(pcWriteBuffer, "\"\r\n");
					}
					else if (ret == MMCONFIG_ERR_NOT_FOUND) {
						strcat(pcWriteBuffer, "No current SSID.\r\n");
					}

					strcat(pcWriteBuffer, " New SSID\t : \"");
					strncat(pcWriteBuffer, pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1]);
					strcat(pcWriteBuffer, "\"\r\n");

					ret = config_write_str(CLI_SET_SSID, pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1]);

					if (ret == MMCONFIG_OK) {
						strcat(pcWriteBuffer, " Status\t\t : SUCCESS - SSID updated!\r\n");
					}
					else {
						strcat(pcWriteBuffer, " Status\t\t : FAIL - SSID update failed!\r\n");
						xReturn = pdFALSE;
					}
				}
				else {
					strcat(pcWriteBuffer, \
							" Invalid SSID!!!\r\n" \
							" Length only allow up to 31 characters.\r\n" \
							" Only allow character: A-Z, a-z, 0-9, !@#$%%^&*()-_=+[]{}|;:'\",.<>?/\r\n");
					xReturn = pdFALSE;
				}

				break;

			case CLI_SET_PASSWORD:
				sprintf(pcWriteBuffer, "\r\n[Set Password]\r\n");

				/* Valid password format */
				if (isValidPassword(pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1])) {
					char passphrase[MAX_PASSWORD_LEN];
					int ret = config_read_str(CLI_SET_PASSWORD, passphrase, sizeof(passphrase));

					if (ret > 0) {
						strcat(pcWriteBuffer, " Current password\t : \"");
						strncat(pcWriteBuffer, passphrase, ret);
						strcat(pcWriteBuffer, "\"\r\n");
					}
					else if (ret == MMCONFIG_ERR_NOT_FOUND) {
						strcat(pcWriteBuffer, "No current password.\r\n");
					}

					strcat(pcWriteBuffer, " New password\t\t : \"");
					strncat(pcWriteBuffer, pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1]);
					strcat(pcWriteBuffer, "\"\r\n");

					ret = config_write_str(CLI_SET_PASSWORD, pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1]);

					if (ret == MMCONFIG_OK) {
						strcat(pcWriteBuffer, " Status\t\t\t : SUCCESS - password updated!\r\n");
					}
					else {
						strcat(pcWriteBuffer, " Status\t\t\t : FAIL - password update failed!\r\n");
						xReturn = pdFALSE;
					}
				}
				else {
					strcat(pcWriteBuffer, \
							" Invalid password!!!\r\n" \
							" Length only allow 8 ~ 63 characters.\r\n" \
							" Only allow character: A-Z, a-z, 0-9, !@#$%%^&*()-_=+[]{}|;:'\",.<>?/\r\n");
					xReturn = pdFALSE;
				}

				break;

			case CLI_SET_SECURITY:
				sprintf(pcWriteBuffer, "\r\n[Set Security]\r\n");

				/* Valid security mode format */
				if (isValidSecurity(pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1])) {
					char security_str[BUFFER_SIZE_SHORT]; // 16
					int ret = config_read_str(CLI_SET_SECURITY, security_str, BUFFER_SIZE_SHORT);

					if (ret > 0) {
						strcat(pcWriteBuffer, " Current security mode\t : \"");
						strncat(pcWriteBuffer, (char *)security_str, ret);
						strcat(pcWriteBuffer, "\"\r\n");
					}
					else if (ret == MMCONFIG_ERR_NOT_FOUND) {
						strcat(pcWriteBuffer, "No current security setting.\r\n");
					}

					strcat(pcWriteBuffer, " New security mode\t : \"");
					strncat(pcWriteBuffer, pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1]);
					strcat(pcWriteBuffer, "\"\r\n");

					ret = config_write_str(CLI_SET_SECURITY, pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1]);

					if (ret == MMCONFIG_OK) {
						strcat(pcWriteBuffer, " Status\t\t\t : SUCCESS - security mode updated!\r\n");
					}
					else {
						strcat(pcWriteBuffer, " Status\t\t\t : FAIL - security mode update failed!\r\n");
						xReturn = pdFALSE;
					}
				}
				else {
					strcat(pcWriteBuffer, \
							" Invalid security mode!!!\r\n" \
							" Only allow \"sae\", \"owe\", \"open\"\r\n");
					xReturn = pdFALSE;
				}

				break;

#if ENABLE_CHANGE_COUNTRY
			case CLI_SET_COUNTRY:
				sprintf(pcWriteBuffer, "\r\n[Set Country]\r\n");

				/* Valid Country code format */
				if (isValidCountry(pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1])) {
					char country_str[BUFFER_SIZE_SHORT]; // 16
					int ret = config_read_str(CLI_SET_COUNTRY, country_str, BUFFER_SIZE_SHORT);

					if (ret > 0) {
						strcat(pcWriteBuffer, " Current country code\t : \"");
						strncat(pcWriteBuffer, (char *)country_str, ret);
						strcat(pcWriteBuffer, "\"\r\n");
					}
					else if (ret == MMCONFIG_ERR_NOT_FOUND) {
						strcat(pcWriteBuffer, "No current counrty code.\r\n");
					}

					strcat(pcWriteBuffer, " New country code\t : \"");
					strncat(pcWriteBuffer, pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1]);
					strcat(pcWriteBuffer, "\"\r\n");

					ret = config_write_str(CLI_SET_COUNTRY, pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1]);

					if (ret == MMCONFIG_OK) {
						strcat(pcWriteBuffer, " Status\t\t\t : SUCCESS - country code updated!\r\n");
					}
					else {
						strcat(pcWriteBuffer, " Status\t\t\t : FAIL - country code update failed!\r\n");
						xReturn = pdFALSE;
					}
				}
				else {
					strcat(pcWriteBuffer, \
							" Invalid country code!!!\r\n" \
							" Length only allow 2 characters.\r\n");
					xReturn = pdFALSE;
				}

				break;
#endif

			case CLI_SET_DHCP:
				sprintf(pcWriteBuffer, "\r\n[Set DHCP]\r\n");

				// Valid DHCP format
				if (isValidBool(pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1])) {
					bool boolval;
					int ret = config_read_bool(CLI_SET_DHCP, &boolval);

					if (ret == MMCONFIG_OK) {
						strcat(pcWriteBuffer, " Current DHCP\t : ");
						strcat(pcWriteBuffer, (boolval == true ? "1 (on)\r\n" : "0 (off)\r\n"));
					}
					else if (ret == MMCONFIG_ERR_NOT_FOUND) {
						strcat(pcWriteBuffer, "No current DHCP setting.\r\n");
					}
					#if 0
					else if (ret == MMCONFIG_ERR_INVALID_KEY) {
						stract(pcWriteBuffer, "key is invalid\r\n");
					}
					else if (ret == ) {
						stract(pcWriteBuffer, "The data pointed to by the key is not an integer represented as a string\r\n");
					}
					#endif
					if (strncmp(pcParams[PARAM_ARG_1], "0", 1) == 0)
						boolval = false;
					else if (strncmp(pcParams[PARAM_ARG_1], "1", 1) == 0)
						boolval = true;

					strcat(pcWriteBuffer, " New DHCP\t : ");
					strcat(pcWriteBuffer, (boolval == true ? "1 (on)\r\n" : "0 (off)\r\n"));

					ret = config_write_bool(CLI_SET_DHCP, boolval);

					if (ret == MMCONFIG_OK) {
						strcat(pcWriteBuffer, " Status\t\t : SUCCESS - DHCP updated!\r\n");
					}
					else {
						strcat(pcWriteBuffer, " Status\t\t : FAIL - DHCP update failed!\r\n");
						xReturn = pdFALSE;
					}
				}
				else {
					strcat(pcWriteBuffer, \
							" Invalid DHCP mode!!!\r\n" \
							" Only allow character '0' or '1'\r\n");
					xReturn = pdFALSE;
				}

				break;

			case CLI_SET_IP:
				sprintf(pcWriteBuffer, "\r\n[Set IP]\r\n");

				/* Valid IP address format */
				if (isValidIPv4(pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1])) {
					char addr_str[MAX_ADDR_STR_LEN];
					int ret = config_read_str(CLI_SET_IP, addr_str, MAX_ADDR_STR_LEN);

					if (ret > 0) {
						strcat(pcWriteBuffer, " Current IP\t : \"");
						strncat(pcWriteBuffer, (char *)addr_str, ret);
						strcat(pcWriteBuffer, "\"\r\n");
					}
					else if (ret == MMCONFIG_ERR_NOT_FOUND) {
						strcat(pcWriteBuffer, "No current IP address setting.\r\n");
					}

					strcat(pcWriteBuffer, " New IP\t\t : \"");
					strncat(pcWriteBuffer, pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1]);
					strcat(pcWriteBuffer, "\"\r\n");

					ret = config_write_str(CLI_SET_IP, pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1]);

					if (ret == MMCONFIG_OK) {
						strcat(pcWriteBuffer, " Status\t\t : SUCCESS - IP address updated!\r\n");
					}
					else {
						strcat(pcWriteBuffer, " Status\t\t : FAIL - IP address update failed!\r\n");
						xReturn = pdFALSE;
					}
				}
				else {
					strcat(pcWriteBuffer, \
							" Invalid IP address!!!\r\n" \
							" Only allow 0 ~ 255 in xxx.xxx.xxx.xxx.\r\n");
					xReturn = pdFALSE;
				}

				break;

			case CLI_SET_NETMASK:
				sprintf(pcWriteBuffer, "\r\n[Set Netmask]\r\n");

				/* Valid netmask format */
				if (isValidIPv4(pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1])) {
					char addr_str[MAX_ADDR_STR_LEN];
					int ret = config_read_str(CLI_SET_NETMASK, addr_str, MAX_ADDR_STR_LEN);

					if (ret > 0) {
						strcat(pcWriteBuffer, " Current netmask : \"");
						strncat(pcWriteBuffer, (char *)addr_str, ret);
						strcat(pcWriteBuffer, "\"\r\n");
					}
					else if (ret == MMCONFIG_ERR_NOT_FOUND) {
						strcat(pcWriteBuffer, "No current netmask.\r\n");
					}

					strcat(pcWriteBuffer, " New netmask\t : \"");
					strncat(pcWriteBuffer, pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1]);
					strcat(pcWriteBuffer, "\"\r\n");

					ret = config_write_str(CLI_SET_NETMASK, pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1]);

					if (ret == MMCONFIG_OK) {
						strcat(pcWriteBuffer, " Status\t\t : SUCCESS - netmask updated!\r\n");
					}
					else {
						strcat(pcWriteBuffer, " Status\t\t : FAIL - netmask update failed!\r\n");
						xReturn = pdFALSE;
					}
				}
				else {
					strcat(pcWriteBuffer, \
							" Invalid netmask!!!\r\n" \
							" Only allow 0 ~ 255 in xxx.xxx.xxx.xxx.\r\n");
					xReturn = pdFALSE;
				}

				break;

			case CLI_SET_GATEWAY:
				sprintf(pcWriteBuffer, "\r\n[Set Gateway]\r\n");

				/* Valid gateway format */
				if (isValidIPv4(pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1])) {
					char addr_str[MAX_ADDR_STR_LEN];
					int ret = config_read_str(CLI_SET_GATEWAY, addr_str, MAX_ADDR_STR_LEN);

					if (ret > 0) {
						strcat(pcWriteBuffer, " Current gateway : \"");
						strncat(pcWriteBuffer, (char *)addr_str, ret);
						strcat(pcWriteBuffer, "\"\r\n");
					}
					else if (ret == MMCONFIG_ERR_NOT_FOUND) {
						strcat(pcWriteBuffer, "No current gateway.\r\n");
					}

					strcat(pcWriteBuffer, " New gateway\t : \"");
					strncat(pcWriteBuffer, pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1]);
					strcat(pcWriteBuffer, "\"\r\n");

					ret = config_write_str(CLI_SET_GATEWAY, pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1]);

					if (ret == MMCONFIG_OK) {
						strcat(pcWriteBuffer, " Status\t\t : SUCCESS - gateway updated!\r\n");
					}
					else {
						strcat(pcWriteBuffer, " Status\t\t : FAIL - gateway update failed!\r\n");
						xReturn = pdFALSE;
					}
				}
				else {
					strcat(pcWriteBuffer, \
							" Invalid gateway!!!\r\n" \
							" Only allow 0 ~ 255 in xxx.xxx.xxx.xxx.\r\n");
					xReturn = pdFALSE;
				}

				break;
			case CLI_SET_POWERSAVE:
				sprintf(pcWriteBuffer, "\r\n[Set Power-save]\r\n");

				/* Valid power save format */
				if (isValidBool(pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1])) {
					bool boolval = false;
					char strval[BUFFER_SIZE];
					int ret = config_read_str(CLI_SET_POWERSAVE, strval, BUFFER_SIZE);

					if (ret > 0) {
						strcat(pcWriteBuffer, " Current powersave mode\t : ");

						if (strncmp(POWERSAVE_ENABLE_STR, strval, sizeof(strval)) == 0) {
							strcat(pcWriteBuffer, "1 ("POWERSAVE_ENABLE_STR")");
						}
						else if (strncmp(POWERSAVE_DISABLE_STR, strval, sizeof(strval)) == 0) {
							strcat(pcWriteBuffer, "0 ("POWERSAVE_DISABLE_STR")");
						}

						strcat(pcWriteBuffer, "\r\n");
					}
					else if (ret == MMCONFIG_ERR_NOT_FOUND) {
						strcat(pcWriteBuffer, "No current powersave mode setting.\r\n");
					}
					else {
						strcat(pcWriteBuffer, "Error read key for powersave mode\r\n");
					}

					if (strncmp(pcParams[PARAM_ARG_1], "0", 1) == 0) {
						boolval = false;
					}
					else if (strncmp(pcParams[PARAM_ARG_1], "1", 1) == 0) {
						boolval = true;
					}

					strcat(pcWriteBuffer, " New powersave mode\t : ");
					strcat(pcWriteBuffer, (boolval == true ? "1 ("POWERSAVE_ENABLE_STR")\r\n" : "0 ("POWERSAVE_DISABLE_STR")\r\n"));

					if (boolval) {
						ret = config_write_str(CLI_SET_POWERSAVE, POWERSAVE_ENABLE_STR, strlen(POWERSAVE_ENABLE_STR));
					}
					else {
						ret = config_write_str(CLI_SET_POWERSAVE, POWERSAVE_DISABLE_STR, strlen(POWERSAVE_DISABLE_STR));
					}

					if (ret == MMCONFIG_OK) {
						strcat(pcWriteBuffer, " Status\t\t\t : SUCCESS - power save updated!\r\n");
					}
					else {
						strcat(pcWriteBuffer, " Status\t\t\t : FAIL -  power save update failed!\r\n");
						xReturn = pdFALSE;
					}
				}
				else {
					strcat(pcWriteBuffer, \
							" Invalid power save mode!!!\r\n" \
							" Only allow character '0' or '1'\r\n");
					xReturn = pdFALSE;
				}

				break;
#if MODBUS_NUM != 0
			case CLI_SET_BAUDRATE:
				sprintf(pcWriteBuffer, "\r\n[Set Device Baudrate]\r\n");

				if (isValidDevice(pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1])) {
					if (isValidBaudrate(pcParams[PARAM_ARG_2], (size_t)xParamStrLen[PARAM_ARG_2])) {
						char strval[BUFFER_SIZE];
						int ret = config_read_str_of_dev(CLI_SET_BAUDRATE, strval, BUFFER_SIZE, pcParams[PARAM_ARG_1]);
						strcat(pcWriteBuffer, " Target device\t\t : \"");
						strncat(pcWriteBuffer, pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1]);
						strcat(pcWriteBuffer, "\"\r\n");

						if (ret > 0) {
							strcat(pcWriteBuffer, " Current baudrate\t : \"");
							strcat(pcWriteBuffer, strval);
							strcat(pcWriteBuffer, "\"\r\n");
						}
						else if (ret == MMCONFIG_ERR_NOT_FOUND) {
							strcat(pcWriteBuffer, "No current baudrate setting of this device.\r\n");
						}
						else {
							strcat(pcWriteBuffer, "Error read key error for baudrate.\r\n");
						}

						strcat(pcWriteBuffer, " New baudrate\t\t : \"");
						strncat(pcWriteBuffer, pcParams[PARAM_ARG_2], (size_t)xParamStrLen[PARAM_ARG_2]);
						strcat(pcWriteBuffer, "\"\r\n");

						ret = config_write_str_of_dev(CLI_SET_BAUDRATE, pcParams[PARAM_ARG_2], (size_t)xParamStrLen[PARAM_ARG_2], pcParams[PARAM_ARG_1]);
						if (ret == MMCONFIG_OK) {
							strcat(pcWriteBuffer, " Status\t\t\t : SUCCESS - baudrate updated!\r\n");
						}
						else {
							strcat(pcWriteBuffer, " Status\t\t\t : FAIL - baudrate update failed!\r\n");
							xReturn = pdFALSE;
						}
					}
					else {
						strcat(pcWriteBuffer, \
							" Invalid Baudrate!!!\r\n" \
							" Only allow 9600, 19200, 38400, 57600, 115200.\r\n");
						xReturn = pdFALSE;
					}
				}
				else {
					strcat(pcWriteBuffer, \
							" Invalid Device!!!\r\n"
							" Only allow 1");
#if MODBUS_NUM > 1
					strcat(pcWriteBuffer, " ~ 4");
#endif
					strcat(pcWriteBuffer, ".\r\n");
					xReturn = pdFALSE;
				}
				break;
			case CLI_SET_PARITY_BIT:
				sprintf(pcWriteBuffer, "\r\n[Set Device Parity Bit]\r\n");

				if (isValidDevice(pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1])) {
					if (isValidParityBit(pcParams[PARAM_ARG_2], (size_t)xParamStrLen[PARAM_ARG_2])) {
						char strval[BUFFER_SIZE];
						int ret = config_read_str_of_dev(CLI_SET_PARITY_BIT, strval, BUFFER_SIZE, pcParams[PARAM_ARG_1]);
						strcat(pcWriteBuffer, " Target device\t\t : \"");
						strncat(pcWriteBuffer, pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1]);
						strcat(pcWriteBuffer, "\"\r\n");

						if (ret > 0) {
							strcat(pcWriteBuffer, " Current Parity Bit\t : \"");
							strcat(pcWriteBuffer, strval);
							strcat(pcWriteBuffer, "\"\r\n");
						}
						else if (ret == MMCONFIG_ERR_NOT_FOUND) {
							strcat(pcWriteBuffer, "No current parity bit setting of this device.\r\n");
						}
						else {
							strcat(pcWriteBuffer, "Error read key error for parity bit.\r\n");
						}

						strcat(pcWriteBuffer, " New Parity Bit\t\t : \"");
						strncat(pcWriteBuffer, pcParams[PARAM_ARG_2], (size_t)xParamStrLen[PARAM_ARG_2]);
						strcat(pcWriteBuffer, "\"\r\n");

						ret = config_write_str_of_dev(CLI_SET_PARITY_BIT, pcParams[PARAM_ARG_2], (size_t)xParamStrLen[PARAM_ARG_2], pcParams[PARAM_ARG_1]);
						if (ret == MMCONFIG_OK) {
							strcat(pcWriteBuffer, " Status\t\t\t : SUCCESS - parity bit updated!\r\n");
						}
						else {
							strcat(pcWriteBuffer, " Status\t\t\t : FAIL - parity bit update failed!\r\n");
							xReturn = pdFALSE;
						}
					}
					else {
						strcat(pcWriteBuffer, \
							" Invalid Parity Bit!!!\r\n" \
							" Only allow none, even, odd.\r\n");
						xReturn = pdFALSE;
					}
				}
				else {
					strcat(pcWriteBuffer, \
							" Invalid Device!!!\r\n"
							" Only allow 1");
#if MODBUS_NUM > 1
					strcat(pcWriteBuffer, " ~ 4");
#endif
					strcat(pcWriteBuffer, ".\r\n");
					xReturn = pdFALSE;
				}
				break;
			case CLI_SET_DATA_BIT:
				sprintf(pcWriteBuffer, "\r\n[Set Device Data Bit]\r\n");

				if (isValidDevice(pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1])) {
					if (isValidDataBit(pcParams[PARAM_ARG_2], (size_t)xParamStrLen[PARAM_ARG_2])) {
						char strval[BUFFER_SIZE];
						int ret = config_read_str_of_dev(CLI_SET_DATA_BIT, strval, BUFFER_SIZE, pcParams[PARAM_ARG_1]);
						strcat(pcWriteBuffer, " Target device\t\t : \"");
						strncat(pcWriteBuffer, pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1]);
						strcat(pcWriteBuffer, "\"\r\n");

						if (ret > 0) {
							strcat(pcWriteBuffer, " Current Data Bit\t : \"");
							strcat(pcWriteBuffer, strval);
							strcat(pcWriteBuffer, "\"\r\n");
						}
						else if (ret == MMCONFIG_ERR_NOT_FOUND) {
							strcat(pcWriteBuffer, "No current data bit setting of this device.\r\n");
						}
						else {
							strcat(pcWriteBuffer, "Error read key error for data bit.\r\n");
						}

						strcat(pcWriteBuffer, " New Data Bit\t\t : \"");
						strncat(pcWriteBuffer, pcParams[PARAM_ARG_2], (size_t)xParamStrLen[PARAM_ARG_2]);
						strcat(pcWriteBuffer, "\"\r\n");

						ret = config_write_str_of_dev(CLI_SET_DATA_BIT, pcParams[PARAM_ARG_2], (size_t)xParamStrLen[PARAM_ARG_2], pcParams[PARAM_ARG_1]);
						if (ret == MMCONFIG_OK) {
							strcat(pcWriteBuffer, " Status\t\t\t : SUCCESS - data bit updated!\r\n");
						}
						else {
							strcat(pcWriteBuffer, " Status\t\t\t : FAIL - data bit update failed!\r\n");
							xReturn = pdFALSE;
						}
					}
					else {
						strcat(pcWriteBuffer, \
							" Invalid Data Bit!!!\r\n" \
							" Only allow 7, 8, 9.\r\n");
						xReturn = pdFALSE;
					}
				}
				else {
					strcat(pcWriteBuffer, \
							" Invalid Device!!!\r\n"
							" Only allow 1");
#if MODBUS_NUM > 1
					strcat(pcWriteBuffer, " ~ 4");
#endif
					strcat(pcWriteBuffer, ".\r\n");
					xReturn = pdFALSE;
				}
				break;
			case CLI_SET_STOP_BIT:
				sprintf(pcWriteBuffer, "\r\n[Set Device Stop Bit]\r\n");

				if (isValidDevice(pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1])) {
					if (isValidStopBit(pcParams[PARAM_ARG_2], (size_t)xParamStrLen[PARAM_ARG_2])) {
						char strval[BUFFER_SIZE];
						int ret = config_read_str_of_dev(CLI_SET_STOP_BIT, strval, BUFFER_SIZE, pcParams[PARAM_ARG_1]);
						strcat(pcWriteBuffer, " Target device\t\t : \"");
						strncat(pcWriteBuffer, pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1]);
						strcat(pcWriteBuffer, "\"\r\n");

						if (ret > 0) {
							strcat(pcWriteBuffer, " Current Stop Bit\t : \"");
							strcat(pcWriteBuffer, strval);
							strcat(pcWriteBuffer, "\"\r\n");
						}
						else if (ret == MMCONFIG_ERR_NOT_FOUND) {
							strcat(pcWriteBuffer, "No current stop bit setting of this device.\r\n");
						}
						else {
							strcat(pcWriteBuffer, "Error read key error for stop bit.\r\n");
						}

						strcat(pcWriteBuffer, " New Stop Bit\t\t : \"");
						strncat(pcWriteBuffer, pcParams[PARAM_ARG_2], (size_t)xParamStrLen[PARAM_ARG_2]);
						strcat(pcWriteBuffer, "\"\r\n");

						ret = config_write_str_of_dev(CLI_SET_STOP_BIT, pcParams[PARAM_ARG_2], (size_t)xParamStrLen[PARAM_ARG_2], pcParams[PARAM_ARG_1]);
						if (ret == MMCONFIG_OK) {
							strcat(pcWriteBuffer, " Status\t\t\t : SUCCESS - stop bit updated!\r\n");
						}
						else {
							strcat(pcWriteBuffer, " Status\t\t\t : FAIL - stop bit update failed!\r\n");
							xReturn = pdFALSE;
						}
					}
					else {
						strcat(pcWriteBuffer, \
							" Invalid Stop Bit!!!\r\n" \
							" Only allow 1, 1.5, 2.\r\n");
						xReturn = pdFALSE;
					}
				}
				else {
					strcat(pcWriteBuffer, \
							" Invalid Device!!!\r\n"
							" Only allow 1");
#if MODBUS_NUM > 1
					strcat(pcWriteBuffer, " ~ 4");
#endif
					strcat(pcWriteBuffer, ".\r\n");
							
					xReturn = pdFALSE;
				}
				break;
			case CLI_SET_MODBUS_PORT:
				sprintf(pcWriteBuffer, "\r\n[Set Modbus Port]\r\n");

				if (isValidDevice(pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1])) {
					if (isValidModbusPort(pcParams[PARAM_ARG_2], (size_t)xParamStrLen[PARAM_ARG_2])) {
						char strval[BUFFER_SIZE];
						int ret = config_read_str_of_dev(CLI_SET_MODBUS_PORT, strval, BUFFER_SIZE, pcParams[PARAM_ARG_1]);
						strcat(pcWriteBuffer, " Target device\t\t : \"");
						strncat(pcWriteBuffer, pcParams[PARAM_ARG_1], (size_t)xParamStrLen[PARAM_ARG_1]);
						strcat(pcWriteBuffer, "\"\r\n");

						if (ret > 0) {
							strcat(pcWriteBuffer, " Current Modbus Port\t : \"");
							strcat(pcWriteBuffer, strval);
							strcat(pcWriteBuffer, "\"\r\n");
						}
						else if (ret == MMCONFIG_ERR_NOT_FOUND) {
							strcat(pcWriteBuffer, "No current modbus port setting of this device.\r\n");
						}
						else {
							strcat(pcWriteBuffer, "Error read key error for modbus port.\r\n");
						}

						strcat(pcWriteBuffer, " New Modbus Port\t : \"");
						strncat(pcWriteBuffer, pcParams[PARAM_ARG_2], (size_t)xParamStrLen[PARAM_ARG_2]);
						strcat(pcWriteBuffer, "\"\r\n");

						ret = config_write_str_of_dev(CLI_SET_MODBUS_PORT, pcParams[PARAM_ARG_2], (size_t)xParamStrLen[PARAM_ARG_2], pcParams[PARAM_ARG_1]);
						if (ret == MMCONFIG_OK) {
							strcat(pcWriteBuffer, " Status\t\t\t : SUCCESS - modbus port updated!\r\n");
						}
						else {
							strcat(pcWriteBuffer, " Status\t\t\t : FAIL - modbus port update failed!\r\n");
							xReturn = pdFALSE;
						}
					}
					else {
						strcat(pcWriteBuffer, \
							" Invalid Modbus Port!!!\r\n" \
							" Only allow 0 ~ 65535.\r\n");
						xReturn = pdFALSE;
					}
				}
				else {
					strcat(pcWriteBuffer, \
							" Invalid Device!!!\r\n"
							" Only allow 1");
#if MODBUS_NUM > 1
					strcat(pcWriteBuffer, " ~ 4");
#endif
					strcat(pcWriteBuffer, ".\r\n");
					xReturn = pdFALSE;
				}
				break;
#endif
			default:
				sprintf(pcWriteBuffer, " - Error command to set configuration.\r\n");
				break;
		}
		strcat(pcWriteBuffer, "\r\n");
	}
	else {
		uxFirstParam = 1U;
		uxParamNumber = 2U;
	}
#if 0
	/* If this is the last of the three parameters then there are no more
	strings to return after this one. */
	if( uxParamNumber == 3U )
	{
		/* If this is the last of the three parameters then there are no more
		strings to return after this one. */
		xReturn = pdFALSE;
		uxFirstParam = 0U;
		uxParamNumber = 0U;
	}
	else
	{
		/* There are more parameters to return after this one. */
		xReturn = pdFALSE;
	}
#endif
	return xReturn;
}

extern void vUARTCommandConsoleStart();
void cfg_start(void)
{
#ifdef STM_USBD_ACM_ENABLE
	MX_USB_DEVICE_Init();
#else
#error "STM_USBD_ACM_ENABLE no define."
#endif

	vCheckCommandLineDefaultValues();
	vRegisterSampleCLICommands();
    vUARTCommandConsoleStart(256, (UBaseType_t) NULL);
}

