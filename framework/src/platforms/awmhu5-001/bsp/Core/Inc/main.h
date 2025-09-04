/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u5xx_hal.h"
#include "stm32u5xx_ll_dma.h"
#include "stm32u5xx_ll_lptim.h"
#include "stm32u5xx_ll_rtc.h"
#include "stm32u5xx_ll_rcc.h"
#include "stm32u5xx_ll_spi.h"
#include "stm32u5xx_ll_usart.h"
#include "stm32u5xx_ll_system.h"
#include "stm32u5xx_ll_gpio.h"
#include "stm32u5xx_ll_exti.h"
#include "stm32u5xx_ll_lpgpio.h"
#include "stm32u5xx_ll_bus.h"
#include "stm32u5xx_ll_cortex.h"
#include "stm32u5xx_ll_utils.h"
#include "stm32u5xx_ll_pwr.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "arf_platform.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
#define SPI_PERIPH (SPI1)
#define SPI_DMA_PERIPH (GPDMA1)
#define SPI_RX_DMA_CHANNEL  (LL_DMA_CHANNEL_14)
#define SPI_TX_DMA_CHANNEL  (LL_DMA_CHANNEL_15)

#define SPI_IRQn            (EXTI1_IRQn)
#define SPI_IRQ_LINE        (LL_EXTI_LINE_1)
#define SPI_IRQ_HANDLER     EXTI1_IRQHandler
#define BUSY_IRQn           (EXTI4_IRQn)
#define BUSY_IRQ_LINE       (LL_EXTI_LINE_4)
#define BUSY_IRQ_HANDLER    EXTI4_IRQHandler

#define LOG_USART               (USART1)
#define LOG_USART_IRQ           (USART1_IRQn)
#define LOG_USART_IRQ_HANDLER   USART1_IRQHandler
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define UART_TX_Pin LL_GPIO_PIN_0
#define UART_TX_GPIO_Port GPIOA
#define UART_RX_Pin LL_GPIO_PIN_1
#define UART_RX_GPIO_Port GPIOA
#define WAKE_Pin LL_GPIO_PIN_2
#define WAKE_GPIO_Port GPIOA
#define RESET_N_Pin LL_GPIO_PIN_3
#define RESET_N_GPIO_Port GPIOA
#define BUSY_Pin LL_GPIO_PIN_4
#define BUSY_GPIO_Port GPIOA
#define BUSY_EXTI_IRQn EXTI4_IRQn
#define SPI_SCK_Pin LL_GPIO_PIN_5
#define SPI_SCK_GPIO_Port GPIOA
#define SPI_MISO_Pin LL_GPIO_PIN_6
#define SPI_MISO_GPIO_Port GPIOA
#define SPI_MOSI_Pin LL_GPIO_PIN_7
#define SPI_MOSI_GPIO_Port GPIOA
#define IO_PB0_Pin LL_GPIO_PIN_0
#define IO_PB0_GPIO_Port GPIOB
#define SPI_IRQ_Pin LL_GPIO_PIN_1
#define SPI_IRQ_GPIO_Port GPIOB
#define SPI_IRQ_EXTI_IRQn EXTI1_IRQn
#define LED_MSG_Pin LL_GPIO_PIN_8
#define LED_MSG_GPIO_Port GPIOA
#define LOG_USART_TX_Pin LL_GPIO_PIN_9
#define LOG_USART_TX_GPIO_Port GPIOA
#define LOG_USART_RX_Pin LL_GPIO_PIN_10
#define LOG_USART_RX_GPIO_Port GPIOA
#define USB_DM_Pin LL_GPIO_PIN_11
#define USB_DM_GPIO_Port GPIOA
#define USB_DP_Pin LL_GPIO_PIN_12
#define USB_DP_GPIO_Port GPIOA
#define SWDIO_Pin LL_GPIO_PIN_13
#define SWDIO_GPIO_Port GPIOA
#define SWCLK_Pin LL_GPIO_PIN_14
#define SWCLK_GPIO_Port GPIOA
#define SPI_CS_Pin LL_GPIO_PIN_6
#define SPI_CS_GPIO_Port GPIOB
#define LED_UR_RX_Pin LL_GPIO_PIN_7
#define LED_UR_RX_GPIO_Port GPIOB
#define LED_UR_TX_Pin LL_GPIO_PIN_8
#define LED_UR_TX_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define DBG_LEVEL 4

#ifndef __FILE_NAME__
#define __FILE_NAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif
#define CRLF  "\r\n"
#define ENDL  "\r\n"

#if DBG_LEVEL >= 4
#define __DBG(fmt_,...) printf("<DBG> %s:%s(%d) "fmt_, __FILE_NAME__,__func__,__LINE__, ##__VA_ARGS__)
#define __MSG(fmt_,...) printf("<MSG> %s:%s(%d) "fmt_, __FILE_NAME__,__func__,__LINE__, ##__VA_ARGS__)
#define __WRN(fmt_,...) printf("<WRN> %s:%s(%d) "fmt_, __FILE_NAME__,__func__,__LINE__, ##__VA_ARGS__)
#define __ERR(fmt_,...) printf("<ERR> %s:%s(%d) "fmt_, __FILE_NAME__,__func__,__LINE__, ##__VA_ARGS__)
#elif DBG_LEVEL == 3
#define __DBG(fmt_,...) printf("<DBG> (%s) "fmt_, __func__, ##__VA_ARGS__)
#define __MSG(fmt_,...) printf("<MSG> (%s) "fmt_, __func__, ##__VA_ARGS__)
#define __WRN(fmt_,...) printf("<WRN> (%s) "fmt_, __func__, ##__VA_ARGS__)
#define __ERR(fmt_,...) printf("<ERR> (%s) "fmt_, __func__, ##__VA_ARGS__)
#elif DBG_LEVEL == 2
#define __DBG(fmt_,...) printf(fmt_, ##__VA_ARGS__)
#define __MSG(fmt_,...) printf(fmt_, ##__VA_ARGS__)
#define __WRN(fmt_,...) printf(fmt_, ##__VA_ARGS__)
#define __ERR(fmt_,...) printf(fmt_, ##__VA_ARGS__)
#elif DBG_LEVEL == 1
#define __DBG(fmt_,...) do {} while (0)
#define __MSG(fmt_,...) printf(fmt_, ##__VA_ARGS__)
#define __WRN(fmt_,...) printf(fmt_, ##__VA_ARGS__)
#define __ERR(fmt_,...) printf(fmt_, ##__VA_ARGS__)
#else //DBG_LEVEL == 0
#define __DBG(fmt_,...) do {} while (0)
#define __MSG(fmt_,...) do {} while (0)
#define __WRN(fmt_,...) do {} while (0)
#define __ERR(fmt_,...) do {} while (0)
#endif
#if DBG_LEVEL > 0
#define __DBG0(fmt_,...)  printf(fmt_, ##__VA_ARGS__)
#define __MSG0(fmt_,...)  printf(fmt_, ##__VA_ARGS__)
#define __WRN0(fmt_,...)  printf(fmt_, ##__VA_ARGS__)
#define __ERR0(fmt_,...)  printf(fmt_, ##__VA_ARGS__)a

extern void printf_uart2(char *buf, char const *fmt, ...);

#define __UART2B(bfr_, fmt_, ...) printf_uart2(bfr_, "%s:%s(%d) "fmt_, __FILE_NAME__, __func__, __LINE__, ##__VA_ARGS__)
#define __UART2(fmt_,...)  printf_uart2(NULL, "%s:%s(%d) "fmt_, __FILE_NAME__, __func__, __LINE__, ##__VA_ARGS__)
#else
#define __DBG0(...)     do {} while (0)
#define __MSG0(...)     do {} while (0)
#define __WRN0(...)     do {} while (0)
#define __ERR0(...)     do {} while (0)

#define __UART2B(...)   do {} while (0)
#define __UART2(...)    do {} while (0)
#endif

#define PLATFORM_NAME				PLATFORM_NAME_AWMHU5_001
#define PLATFORM_RS485_NUM			1
#define PLATFORM_HALOW_HAS_LED		1
#define PLATFORM_HALOW_LED_PORT		LED_MSG_GPIO_Port
#define PLATFORM_HALOW_LED_PIN		LED_MSG_Pin
#define PLATFORM_HALOW_LED_ACTIVE	GPIO_ACTIVE_LOW

#if defined (PLATFORM_RS232_NUM) && defined (PLATFORM_RS485_NUM)
	#define MODBUS_NUM (PLATFORM_RS485_NUM + PLATFORM_RS232_NUM)
#elif defined (PLATFORM_RS232_NUM)
	#define MODBUS_NUM (PLATFORM_RS232_NUM)
#elif defined (PLATFORM_RS485_NUM)
	#define MODBUS_NUM (PLATFORM_RS485_NUM)
#endif
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
