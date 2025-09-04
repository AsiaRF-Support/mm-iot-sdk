#ifndef __SHT30_H_
#define __SHT30_H_

#include <math.h>

void SHT30_Reset(void);
uint8_t SHT30_Init(void);
uint8_t SHT30_Read_Dat(uint8_t *dat);
uint8_t CheckCrc8(const uint8_t *message, uint8_t initial_value);
uint8_t SHT30_Dat_To_Float(const uint8_t *dat, float_t *temperature, float_t *humidity);

#endif // __SHT30_H_
