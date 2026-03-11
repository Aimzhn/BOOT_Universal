
/**
 ******************************************************************************
 * @file    ddflashops.h
 * @brief   This file contains the headers of the flash operations functions.
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

#ifndef FLASH_OPERATIONS_H
#define FLASH_OPERATIONS_H

#include <stdbool.h>
#include <stdint.h>

/* Flash Memory Configuration for STM32F103VCT6 */
#define FLASH_PAGE_SIZE_BOOT 2048 // Flash page size (STM32F103VCT6 is 2KB)
#define FLASH_START_ADDRESS 0x08000000U
#define FLASH_SIZE (256U * 1024UL) // (STM32F103VCT6 is 256KB)
#define FLASH_END_ADDRESS (FLASH_START_ADDRESS + FLASH_SIZE)
#define APP_ADDRESS 0x08008000 // Application start address
#define ERASED_FLASH_VALUE 0xFFFFFFFFU

//SRAM Configuration
#define SRAM_START_ADDRESS 0x20000000U
#define SRAM_SIZE (48U * 1024UL) // 48KB
#define SRAM_END_ADDRESS (SRAM_START_ADDRESS + SRAM_SIZE)

/* Application Address Configuration */
#define APP_START_ADDRESS 0x08008000       // Bootloader reserves 32KB
#define APP_MAX_SIZE (FLASH_SIZE - 0x8000) // 224KB for app

#define APP_VALID_FLAG 0xABABABAB
#define APP_FLAG_ADDRESS (APP_START_ADDRESS + APP_MAX_SIZE - 4)

/* Flash Operation Error Codes */
typedef enum {
    FLASH_OK = 0,
    FLASH_ERROR_LOCKED,   // Flash locked
    FLASH_ERROR_ADDRESS,  // Invalid address
    FLASH_ERROR_ERASE,    // Erase failed
    FLASH_ERROR_WRITE,    // Write failed
    FLASH_ERROR_VERIFY,   // Verify failed
    FLASH_ERROR_SIZE,     // Invalid size
    FLASH_ERROR_ALIGNMENT // Address not aligned
} FlashError_t;

/* Public Flash Operations */
bool Flash_Erase(uint32_t startAddress, uint32_t numPages);
bool Flash_Write(uint32_t address, uint8_t *data, uint32_t length);
bool Flash_Read(uint32_t address, uint8_t *buffer, uint32_t length);
bool Flash_Verify(uint32_t address, uint8_t *expectedData, uint32_t length);

/* Application Management */
bool Verify_Application(uint32_t address);
void JumpTo_Application(uint32_t address);
bool Flash_IsValidAppAddress(uint32_t address);
uint32_t Flash_GetAppSize(uint32_t appAddress);

/* Flash Information */
uint32_t Flash_GetFreeSpace(uint32_t startAddress);
bool Flash_IsErased(uint32_t address, uint32_t length);
uint32_t Flash_GetPageFromAddress(uint32_t address);

/* Internal Functions (should not be called directly) */
void Flash_Unlock(void);
void Flash_Lock(void);
FlashError_t Flash_GetLastError(void);

#endif // FLASH_OPERATIONS_H