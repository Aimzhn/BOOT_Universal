
/*
 ******************************************************************************
 * @file    flash_operations.c
 * @brief   This file contains the implementation of the flash operations functions.
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

#include "../../Core/Inc/iap/bootloader.h"
#include "../../Core/Inc/iap/flash_operations.h"
#include "stm32f1xx_hal.h"

// Flash unlock
void Flash_Unlock(void)
{
    // Check if already unlocked
    if ((FLASH->CR & FLASH_CR_LOCK) != 0) {
        // Write key sequence
        FLASH->KEYR = 0x45670123; // FLASH_KEY1
        FLASH->KEYR = 0xCDEF89AB; // FLASH_KEY2
    }
}

// Flash lock
void Flash_Lock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

// Erase Flash pages
bool Flash_Erase(uint32_t startAddress, uint32_t numPages)
{
    // Verify address range
    if (startAddress < 0x08000000 || startAddress + (numPages * FLASH_PAGE_SIZE_BOOT) > 0x08040000) {
        return false;
    }

    FLASH_EraseInitTypeDef eraseInit;
    uint32_t pageError = 0;
    HAL_StatusTypeDef status;

    // Unlock Flash
    Flash_Unlock();

    // Wait for Flash not busy
    while (__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY)) {
        // Can add timeout handling
    }

    // Configure erase parameters
    eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    eraseInit.PageAddress = startAddress;
    eraseInit.NbPages = numPages;

    // Execute erase
    status = HAL_FLASHEx_Erase(&eraseInit, &pageError);

    // Clear operation flags
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);

    // Lock Flash
    Flash_Lock();

    return (status == HAL_OK);
}

// Write to Flash
bool Flash_Write(uint32_t address, uint8_t *data, uint32_t length)
{
    // Verify address and length
    if (address < 0x08000000 || address + length > 0x08040000 || length == 0) {
        return false;
    }

    // Ensure address alignment (STM32F1 requires half-word alignment)
    if (address % 2 != 0) {
        return false;
    }

    HAL_StatusTypeDef status;
    uint32_t i;

    // Unlock Flash
    Flash_Unlock();

    // Wait for Flash not busy
    while (__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY)) {
        // Can add timeout handling
    }

    // Write data
    for (i = 0; i < length; i += 2) {
        uint16_t wordData = data[i];

        // If odd length, fill last byte with 0xFF
        if (i + 1 < length) {
            wordData |= (uint16_t)(data[i + 1] << 8);
        } else {
            wordData |= (uint16_t)(0xFF << 8);
        }

        // Write half-word
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address + i, wordData);

        if (status != HAL_OK) {
            // Clear error flags
            __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
            Flash_Lock();
            return false;
        }

        // Wait for write completion
        while (__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY)) {
            // Can add timeout handling
        }

        // Immediately verify write (optional but recommended)
        if (*(volatile uint16_t *)(address + i) != wordData) {
            __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
            Flash_Lock();
            return false;
        }
    }

    // Clear operation flags
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);

    // Lock Flash
    Flash_Lock();
    return true;
}

// Verify application
bool Verify_Application(uint32_t app_start_address)
{
    uint32_t sp;
    uint32_t pc;

    if (app_start_address < APP_START_ADDRESS || app_start_address >= FLASH_END_ADDRESS) {
        return false;
    }

    /* Read vector table */
    sp = *(uint32_t *)app_start_address;
    pc = *(uint32_t *)(app_start_address + 4);

    if (sp == ERASED_FLASH_VALUE || pc == ERASED_FLASH_VALUE) {
        return false;
    }

    if (sp < SRAM_START_ADDRESS || sp > SRAM_END_ADDRESS) {
        return false;
    }

    if (pc < APP_START_ADDRESS || pc >= FLASH_END_ADDRESS) {
        return false;
    }

    return true;
}

void JumpTo_Application(uint32_t address)
{
    typedef void (*pFunction)(void);
    pFunction jump;

    uint32_t appStack;
    uint32_t appEntry;

    appStack = *(volatile uint32_t *)address;
    appEntry = *(volatile uint32_t *)(address + 4);

    SysTick->CTRL = 0;

    SCB->VTOR = address;

    __set_MSP(appStack);

    jump = (pFunction)(appEntry);
    jump();

    while (1)
        ;
}
// Check if Flash region is erased
bool Flash_IsErased(uint32_t address, uint32_t length)
{
    // Verify parameters
    if (address < 0x08000000 || address + length > 0x08040000 || length == 0) {
        return false;
    }

    // Check each word for 0xFFFFFFFF
    for (uint32_t i = 0; i < length; i += 4) {
        if (*(volatile uint32_t *)(address + i) != 0xFFFFFFFF) {
            return false;
        }
    }

    return true;
}