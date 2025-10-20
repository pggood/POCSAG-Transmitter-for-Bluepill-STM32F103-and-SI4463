/*
 * si4463_driver.h
 *
 *  Created on: Oct 20, 2025
 *      Author: peter
 */

#ifndef SI4463_DRIVER_H
#define SI4463_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

// STM32 HAL version - simplified for single instance
void Si4463_Init(SPI_HandleTypeDef *hspi, GPIO_TypeDef* sdn_port, uint16_t sdn_pin,
                 GPIO_TypeDef* nirq_port, uint16_t nirq_pin,
                 GPIO_TypeDef* cs_port, uint16_t cs_pin);
void Si4463_Reset(void);
void Si4463_Configure(void);
void Si4463_Write(uint8_t *data, uint8_t len);
void Si4463_Read(uint8_t *cmd, uint8_t cmdLen, uint8_t *data, uint8_t dataLen);
void Si4463_SetFrequency(float freq);
void Si4463_StartRx(void);
bool Si4463_ReadRxFifo(uint8_t *data, uint8_t len);
void Si4463_GetIntStatus(uint8_t *intStatus);
void Si4463_ClearInt(void);
uint8_t Si4463_GetFifoInfo(void);
bool Si4463_IsNIRQActive(void);

// POCSAG-specific functions
void Si4463_ConfigureForPOCSAG(void);
void Si4463_TransmitPOCSAG(uint8_t *data, uint16_t len);
void Si4463_SetPOCSAGDataRate(uint16_t baudRate);

#endif // SI4463_DRIVER_H
