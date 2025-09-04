#include "sht3x.h"
#include "main.h"
#include "stm32u5xx_hal.h"	// Link to HAL library.
#include "mmosal.h"


/* ADDR Pin Conect to VSS */
#define SHT30_ADDR_WRITE		(0x44 << 1)				// 10001000, SHT30 write address acaccording to user manaul indication.
#define SHT30_ADDR_READ			((0x44 << 1) + 1)		// 10001011, SHT30 read address according to user manual indication.


extern I2C_HandleTypeDef hi2c1;		// Open I2C1 interface.

typedef enum
{
    /* Software reset command */
    SOFT_RESET_CMD = 0x30A2,

    /*
		Single measurement mode
		Name format: Repeatability_CS_CMD
		CS: Clock stretching
    */
    HIGH_ENABLED_CMD    = 0x2C06,
    MEDIUM_ENABLED_CMD  = 0x2C0D,
    LOW_ENABLED_CMD     = 0x2C10,
    HIGH_DISABLED_CMD   = 0x2400,
    MEDIUM_DISABLED_CMD = 0x240B,
    LOW_DISABLED_CMD    = 0x2416,

    /*
		Period measurement mode
		Name format: Repeatability_MPS_CMD
		MPS: measurement per second
    */
    HIGH_0_5_CMD   = 0x2032,
    MEDIUM_0_5_CMD = 0x2024,
    LOW_0_5_CMD    = 0x202F,
    HIGH_1_CMD     = 0x2130,
    MEDIUM_1_CMD   = 0x2126,
    LOW_1_CMD      = 0x212D,
    HIGH_2_CMD     = 0x2236,
    MEDIUM_2_CMD   = 0x2220,
    LOW_2_CMD      = 0x222B,
    HIGH_4_CMD     = 0x2334,
    MEDIUM_4_CMD   = 0x2322,
    LOW_4_CMD      = 0x2329,
    HIGH_10_CMD    = 0x2737,
    MEDIUM_10_CMD  = 0x2721,
    LOW_10_CMD     = 0x272A,

    /* In periaod measurement mode to read data command */
    READOUT_FOR_PERIODIC_MODE = 0xE000,
} SHT30_CMD;


/**
 * @brief    Send one command to SHT30 (16bit).
 * @param    cmd —— SHT30 command.
 * @retval   Return HAL_OK if success.
*/
static uint8_t SHT30_Send_Cmd(SHT30_CMD cmd)
{
    uint8_t cmd_buffer[2];
    cmd_buffer[0] = cmd >> 8;
    cmd_buffer[1] = cmd;
    return HAL_I2C_Master_Transmit(&hi2c1, SHT30_ADDR_WRITE, (uint8_t *)cmd_buffer, 2, 0xFFFF);
}


/**
 * @brief    Reset SHT30.
 * @param    none.
 * @retval   none.
*/
void SHT30_Reset(void)
{
    SHT30_Send_Cmd(SOFT_RESET_CMD);
	// HAL_Delay(20);
	mmosal_task_sleep(20);
}


/**
 * @brief    Initialization SHT30.
 * @param    none.
 * @retval   Return HAL_OK if success.
 * @note     Period measurement mode.
*/
uint8_t SHT30_Init(void)
{
    return SHT30_Send_Cmd(MEDIUM_2_CMD);
}


/**
 * @brief    Read data from SHT30.
 * @param    dat —— Return data's address (6 bits).
 * @retval   Return HAL_OK if success.
*/
uint8_t SHT30_Read_Dat(uint8_t *dat)
{
    SHT30_Send_Cmd(READOUT_FOR_PERIODIC_MODE);
    return HAL_I2C_Master_Receive(&hi2c1, SHT30_ADDR_READ, dat, 6, 0xFFFF);
}


#define CRC8_POLYNOMIAL 0x31

uint8_t CheckCrc8(const uint8_t *message, uint8_t initial_value)
{
    /* Initialization */
    uint8_t remainder = initial_value;

    for(uint8_t j = 0; j < 2;j++)
    {
        remainder ^= message[j];

        /* Count from the highest byte */
        for (uint8_t i = 0; i < 8; i++)
        {
            if (remainder & 0x80)
            {
                remainder = (remainder << 1)^CRC8_POLYNOMIAL;
            }
            else
            {
                remainder = (remainder << 1);
            }
        }
    }

    /* Return CRC */
    return remainder;
}


/**
 * @brief    CRC calibration the data from SHT30, turn into the temperature and humidity.
 * @param    dat  —— Store the address for receiving data.
 * @retval   Return 0 if calibration success.
 *           Return 1 if CRC calibration fail, set the temperature and humidity to 0.
*/
uint8_t SHT30_Dat_To_Float(const uint8_t *dat, float_t *temperature, float_t *humidity)
{
    uint16_t recv_temperature = 0;
    uint16_t recv_humidity = 0;

    /* Check whether the temperature and humidity data are received correctly. */
    if(CheckCrc8(dat, 0xFF) != dat[2] || CheckCrc8(&dat[3], 0xFF) != dat[5])
        return 1;

    /* Turn into temperature data */
    recv_temperature = ((uint16_t)dat[0] << 8) | dat[1];
    *temperature = -45 + 175 * ((float_t)recv_temperature / 65535);

    /* Turn into humidity data */
    recv_humidity = ((uint16_t)dat[3] << 8) | dat[4];
    *humidity = 100 * ((float_t)recv_humidity / 65535);

    return 0;
}
