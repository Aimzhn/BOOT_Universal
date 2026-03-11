/**
 ******************************************************************************
 * @file    crc.c
 * @brief   This file contains the implementation of the CRC functions.
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

#include "../../Core/Inc/dd/crc.h"

// Modbus CRC16 implementation (corresponding to your protocol section 6.2)
uint16_t CRC16_Modbus(const uint8_t *data, uint32_t length)
{
    uint16_t crc = 0xFFFF;

    for (uint32_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i];

        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }

    return crc;
}

// CRC32 calculation (if needed)
uint32_t CRC32_Calculate(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    uint32_t i, j;

    for (i = 0; i < length; i++) {
        crc ^= (uint32_t)data[i];

        for (j = 0; j < 8; j++) {
            if (crc & 0x00000001) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc = crc >> 1;
            }
        }
    }

    return ~crc;
}

// CRC8 calculation (if needed)
uint8_t CRC8_Calculate(const uint8_t *data, uint32_t length)
{
    uint8_t crc = 0x00;
    uint8_t i, j;

    for (i = 0; i < length; i++) {
        crc ^= data[i];

        for (j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc = crc << 1;
            }
        }
    }

    return crc;
}