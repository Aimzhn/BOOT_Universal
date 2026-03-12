/**
 ******************************************************************************
 * @file    ddbootpro.h
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

#ifndef _DD_BOOTLOADER_PROTOCOL_H
#define _DD_BOOTLOADER_PROTOCOL_H

/* Include Headers */
#include "stdbool.h"
#include "stdint.h"

/* Protocol Constants */
#define BOOT_PROTOCOL_FUNCTION_CODE 0x41
#define BOOT_PROTOCOL_ERROR_CODE 0xC1

/* Sub Function Codes */
#define SUB_FUNCTION_LOAD_START 0x01
#define SUB_FUNCTION_LOAD_DATA 0x02
#define SUB_FUNCTION_LOAD_COMPLETE 0x03
#define SUB_FUNCTION_LOAD_ACTIVE 0x04

/* File Type Definitions */
typedef enum {
    FILE_TYPE_BIOS = 0xAF,
    FILE_TYPE_MAIN_DSP = 0xB0,
    FILE_TYPE_AUX_DSP = 0xB1,
    FILE_TYPE_CPLD = 0xB2,
    FILE_TYPE_FPGA = 0xB3,
    FILE_TYPE_TOTAL_PACKAGE = 0xB5
} FileType_t;

/* Error Code Definitions */
typedef enum {
    ERROR_NONE = 0x00,
    ERROR_INVALID_FUNCTION_CODE = 0x01,
    ERROR_INVALID_SUB_FUNCTION = 0x02,
    ERROR_INVALID_DATA_LENGTH = 0x03,
    ERROR_INVALID_FILE_TYPE = 0x04,
    ERROR_INVALID_SEQ_NUM = 0x05,
    ERROR_FLASH_WRITE_FAIL = 0x06,
    ERROR_CRC_CHECK_FAIL = 0x07,
    ERROR_FLASH_ERASE_FAIL = 0x08,
    ERROR_INVALID_ADDRESS = 0x09,
    ERROR_UNKNOWN = 0xFF
} ErrorCode_t;

/* Bootloader State Machine */
typedef enum {
    BOOT_STATE_IDLE,           // Idle state
    BOOT_STATE_LOAD_STARTED,   // Load started
    BOOT_STATE_LOADING_DATA,   // Loading data
    BOOT_STATE_LOAD_COMPLETED, // Load completed
    BOOT_STATE_ERROR           // Error state
} BootState_t;

/* File Information Structure */
typedef struct {
    FileType_t type;        // File type
    uint32_t length;        // Total file length
    uint32_t received;      // Received length
    uint16_t expectedCRC;   // Expected CRC value
    uint16_t calculatedCRC; // Calculated CRC value
    uint8_t frameSize;      // Frame size
    uint32_t flashAddress;  // Flash address
} FileInfo_t;

/* Protocol Frame Structures - Packed to avoid alignment issues */
#pragma pack(push, 1)

/* 7.7.2 Load Start Command */
typedef struct {
    uint8_t functionCode;    // 0x41
    uint8_t subFunctionCode; // 0x01
    uint8_t dataLength;      // 0x06
    uint8_t fileType;        // File type
    uint32_t fileLength;     // File length (4 bytes)
    uint8_t frameLength;     // Frame length
} LoadStartReq_t;

typedef struct {
    uint8_t functionCode;    // 0x41
    uint8_t subFunctionCode; // 0x01
    uint8_t dataLength;      // 0x06
    uint8_t fileType;        // File type
    uint32_t fileLength;     // File length
    uint8_t frameLength;     // Frame length
} LoadStartResp_t;

/* 7.7.3 Load Data Command */
#pragma pack(push, 1)
typedef struct {
    uint8_t functionCode;    // 0x41
    uint8_t subFunctionCode; // 0x02
    uint8_t dataLength;      // 0x04 + N
    uint8_t fileType;        // File type
    uint8_t frameSeqHi;      // Frame sequence number
    uint8_t frameSeqLo;      // Frame sequence number
    uint8_t frameLength;     // N - Frame data length
    // uint8_t data[];        // Frame data (variable length part)
} LoadDataReqHeader_t;
#pragma pack(pop)

typedef struct {
    uint8_t functionCode;    // 0x41
    uint8_t subFunctionCode; // 0x02
    uint8_t dataLength;      // 0x04
    uint8_t fileType;        // File type
    uint16_t frameSeq;       // Frame sequence number
    uint8_t frameLength;     // Frame length
} LoadDataResp_t;

/* 7.7.4 Load Complete Command */
typedef struct {
    uint8_t functionCode;    // 0x41
    uint8_t subFunctionCode; // 0x03
    uint8_t dataLength;      // 0x03
    uint8_t fileType;        // File type
    uint16_t crcChecksum;    // CRC checksum
} LoadCompleteReq_t;

typedef struct {
    uint8_t functionCode;    // 0x41
    uint8_t subFunctionCode; // 0x03
    uint8_t dataLength;      // 0x03
    uint8_t fileType;        // File type
    uint16_t crcChecksum;    // CRC checksum
} LoadCompleteResp_t;

/* 7.7.5 Load Activate Command */
typedef struct {
    uint8_t functionCode;    // 0x41
    uint8_t subFunctionCode; // 0x04
    uint8_t dataLength;      // 0x01
    uint8_t fileType;        // File type
} LoadActivateReq_t;

/* Error Response */
typedef struct {
    uint8_t errorCode;      // 0xC1
    uint8_t errorCodeValue; // Error code value
} ErrorResp_t;

#pragma pack(pop)

#endif // _DD_BOOTLOADER_PROTOCOL_H