/*
 * ModbusConfig.h
 *
 *  Created on: Apr 28, 2021
 *      Author: Alejandro Mera
 *
 *  This is a template for the Modbus library configuration.
 *  Every project needs configure and include a copy of this file renamed to ModbusConfig.h
 */

#ifndef MODBUS_LIB_CONFIG_MODBUSCONFIG_H_
#define MODBUS_LIB_CONFIG_MODBUSCONFIG_H_

#include "main.h"
/* Uncomment the following line to enable support for Modbus RTU over USB CDC profile. Only tested for BluePill f103 board. */
//#define ENABLE_USB_CDC 1

/* Uncomment the following line to enable support for Modbus TCP. Only tested for Nucleo144-F429ZI. */
#define ENABLE_TCP		1

#define ENABLE_DATA_LED	    1


#define T35				5					// Timer T35 period (in ticks) for end frame detection.
#define MAX_BUFFER		(128 * 2)			// Maximum size for the communication buffer in bytes.
#define MAX_M_HANDLERS	(2 * MODBUS_NUM)	//Maximum number of modbus handlers that can work concurrently
#define MAX_TELEGRAMS	2					//Max number of Telegrams in master queue

#define NUMBERTCPCONN   4					// Maximum number of simultaneous client connections, it should be equal or less than LWIP configuration
#define TCPAGINGCYCLES	3		// Number of times the master will check for a incoming request before closing the connection for inactivity
/* Note: the total aging time for a connection is approximately NUMBERTCPCONN*TCPAGINGCYCLES*u16timeOut ticks
 * for the values selected in this example it is approximately 40 seconds
 */

//#define	LWIP_NETCONN	 1
//#define	LWIP_SO_RCVTIMEO 1
//#define	LWIP_SO_SNDTIMEO 1

#endif /* MODBUS_LIB_CONFIG_MODBUSCONFIG_H_ */

