/**
 ******************************************************************************
 * @file    crc.h
 * @brief   This file contains the headers of the CRC functions.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 zhangduoduo/universalzhang/frankzhang.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#ifndef CRC_H
#define CRC_H

#include <stdbool.h>
#include <stdint.h>

// CRC Calculation
uint16_t CRC16_Modbus(const uint8_t *data, uint32_t length);
uint32_t CRC32_Calculate(const uint8_t *data, uint32_t length);
uint8_t CRC8_Calculate(const uint8_t *data, uint32_t length);
uint16_t CRC16_Modbus_Continue(uint16_t crc, const uint8_t *data, uint32_t length);


#endif // CRC_H