
/**
 ******************************************************************************
 * @file    ddboot.h
 * @brief   This file contains the headers of the bootloader functions.
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

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include "ddbootpro.h"
#include <stdbool.h>
#include <stdint.h>

// Bootloader configuration
#define BOOTLOADER_TIMEOUT 1500 // 1.5 seconds timeout

#define RTC_BOOT_MAGIC 0x00005A5A

// RTC备份寄存器索引定义
#define RTC_BKP_MAGIC RTC_BKP_DR1
#define RTC_BKP_STATE RTC_BKP_DR2
#define RTC_BKP_FILE_TYPE RTC_BKP_DR3
#define RTC_BKP_FILE_LEN RTC_BKP_DR4
#define RTC_BKP_FRAME_LEN RTC_BKP_DR5
#define RTC_BKP_CRC RTC_BKP_DR6

// RTC状态定义
#define RTC_STATE_NORMAL 0x00000000
#define RTC_STATE_REQUEST 0x0000A5A5

// RTC Boot信息结构体
typedef struct {
    uint8_t fileType;
    uint32_t fileLength;
    uint8_t frameLength;
} RtcBootInfo_t;

// Function declarations
void Bootloader_Init(void);
void Bootloader_Main(void);
void Bootloader_ProcessFrame(uint8_t *data, uint16_t len);

// Add state query functions
BootState_t Bootloader_GetState(void);
void Bootloader_SetState(BootState_t state);

#endif // BOOTLOADER_H