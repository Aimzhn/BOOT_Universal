/**
 ******************************************************************************
 * @file    bootloader.c
 * @brief   This file contains the implementation of the bootloader functions.
 ******************************************************************************
 */

#include "../../Core/Inc/iap/bootloader.h"
#include "../../Core/Inc/iap/crc.h"
#include "../../Core/Inc/iap/flash_operations.h"
#include "../../Core/Inc/iap/iap_protocol.h"
#include "main.h"
#include "rtc.h"
#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <string.h>

/* External handles */
extern UART_HandleTypeDef huart3;
extern RTC_HandleTypeDef hrtc;
extern volatile uint8_t g_1ms_flag;

/* Global variables */
static BootState_t bootState = BOOT_STATE_IDLE;
static FileInfo_t currentFile;

static uint8_t rxBuffer[512];
static uint16_t rxIndex = 0;
static uint32_t lastRxTime = 0;

static uint8_t rxByte = 0;
static uint8_t frameBuf[512];

/* Frame processing flags */
static volatile uint8_t frameReady = 0;
static volatile uint16_t frameLen = 0;
static volatile uint8_t frameLock = 0;

/* Internal functions */
static void SendResponse(uint8_t *data, uint16_t len);
static void SendErrorResponse(ErrorCode_t error);

static void ProcessLoadStart(uint8_t *data, uint16_t len);
static void ProcessLoadData(uint8_t *data, uint16_t len);
static void ProcessLoadComplete(uint8_t *data, uint16_t len);
static void ProcessLoadActivate(uint8_t *data, uint16_t len);

static void ResetBootloaderState(void);

static bool IsFrameComplete(uint8_t *buffer, uint16_t length);
static void ProcessReceivedFrame(void);

static void UpdateCRC(uint8_t *data, uint16_t length);

static bool ValidateLoadStart(const LoadStartReq_t *req);
static bool ValidateLoadData(const LoadDataReqHeader_t *header, uint16_t dataLen);
static bool ValidateLoadComplete(const LoadCompleteReq_t *req);

static uint8_t RTC_GetBootInfo(RtcBootInfo_t *info);
static void RTC_ClearBootInfo(void);

/* Utility */
static uint32_t Convert_BE32(uint32_t value);

/* ============================================================= */
/* Utility Functions                                              */
/* ============================================================= */

/**
 * @brief Convert 32-bit big-endian value to host endian
 */
static uint32_t Convert_BE32(uint32_t value)
{
    return ((value & 0xFF000000) >> 24) | ((value & 0x00FF0000) >> 8) | ((value & 0x0000FF00) << 8) |
           ((value & 0x000000FF) << 24);
}

/* ============================================================= */
/* RTC Functions                                                  */
/* ============================================================= */

/**
 * @brief Read boot information from RTC backup registers
 */
static uint8_t RTC_GetBootInfo(RtcBootInfo_t *info)
{
    if (info == NULL)
        return 0;

    HAL_PWR_EnableBkUpAccess();

    uint32_t magic = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_MAGIC);
    uint32_t state = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_STATE);

    if (magic != RTC_BOOT_MAGIC || state != RTC_STATE_REQUEST)
        return 0;

    info->fileType = (uint8_t)HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_FILE_TYPE);
    info->fileLength = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_FILE_LEN);
    info->frameLength = (uint8_t)HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_FRAME_LEN);

    return 1;
}

/**
 * @brief Clear RTC upgrade information
 */
static void RTC_ClearBootInfo(void)
{
    HAL_PWR_EnableBkUpAccess();

    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_MAGIC, 0);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_STATE, RTC_STATE_NORMAL);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_FILE_TYPE, 0);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_FILE_LEN, 0);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_FRAME_LEN, 0);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_CRC, 0);
}

/**
 * @brief Check RTC and start bootloader if upgrade requested
 */
void Bootloader_CheckAndStartFromRTC(void)
{
    RtcBootInfo_t rtcInfo;

    if (!RTC_GetBootInfo(&rtcInfo))
        return;

    uint8_t loadStartCmd[sizeof(LoadStartReq_t)] = {0};
    LoadStartReq_t *req = (LoadStartReq_t *)loadStartCmd;

    req->functionCode = BOOT_PROTOCOL_FUNCTION_CODE;
    req->subFunctionCode = SUB_FUNCTION_LOAD_START;
    req->dataLength = 0x06;
    req->fileType = rtcInfo.fileType;
    req->fileLength = rtcInfo.fileLength;
    req->frameLength = rtcInfo.frameLength;

    ProcessLoadStart(loadStartCmd, sizeof(LoadStartReq_t));

    Bootloader_Main();
}

/* ============================================================= */
/* Bootloader Initialization                                      */
/* ============================================================= */

void Bootloader_Init(void)
{
    ResetBootloaderState();

    HAL_UART_Receive_IT(&huart3, &rxByte, 1);

    lastRxTime = HAL_GetTick();
}

/**
 * @brief Reset bootloader state
 */
static void ResetBootloaderState(void)
{
    bootState = BOOT_STATE_IDLE;

    rxIndex = 0;

    memset(&currentFile, 0, sizeof(FileInfo_t));

    memset(rxBuffer, 0, sizeof(rxBuffer));

    lastRxTime = HAL_GetTick();
}

/* ============================================================= */
/* Bootloader Main Loop                                           */
/* ============================================================= */

void Bootloader_Main(void)
{
    uint32_t timeout_tick = HAL_GetTick() + BOOTLOADER_TIMEOUT;

    uint32_t last_rx_index = 0;

    while (1) {

        if (rxIndex != last_rx_index) {

            last_rx_index = rxIndex;

            timeout_tick = HAL_GetTick() + BOOTLOADER_TIMEOUT;
        }

        if (frameReady) {

            frameReady = 0;

            ProcessReceivedFrame();

            rxIndex = 0;

            frameLock = 0;

            HAL_UART_Receive_IT(&huart3, &rxByte, 1);

            timeout_tick = HAL_GetTick() + BOOTLOADER_TIMEOUT;
        }

        if (bootState == BOOT_STATE_IDLE) {

            if (HAL_GetTick() > timeout_tick) {

                if (Verify_Application(APP_ADDRESS)) {

                    JumpTo_Application(APP_ADDRESS);
                } else {

                    timeout_tick = HAL_GetTick() + BOOTLOADER_TIMEOUT;
                }
            }
        }
    }
}

/* ============================================================= */
/* UART Interrupt Callback                                        */
/* ============================================================= */

void Bootloader_UARTCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART3 || frameLock)
        return;

    rxBuffer[rxIndex++] = rxByte;

    if (IsFrameComplete(rxBuffer, rxIndex)) {

        frameLen = rxIndex;

        memcpy(frameBuf, rxBuffer, frameLen);

        frameReady = 1;

        frameLock = 1;

        rxIndex = 0;
    }

    HAL_UART_Receive_IT(&huart3, &rxByte, 1);
}

/* ============================================================= */
/* Communication                                                  */
/* ============================================================= */

static void SendResponse(uint8_t *data, uint16_t len)
{
    PLATFORM_RS485_ENABLE_TX();

    HAL_UART_Transmit(&huart3, data, len, 100);

    while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_TC) == RESET)
        ;

    PLATFORM_RS485_ENABLE_RX();
}

static void SendErrorResponse(ErrorCode_t error)
{
    ErrorResp_t errorResp = {.errorCode = BOOT_PROTOCOL_ERROR_CODE, .errorCodeValue = (uint8_t)error};

    SendResponse((uint8_t *)&errorResp, sizeof(errorResp));

    bootState = BOOT_STATE_ERROR;
}

/* ============================================================= */
/* Frame Processing                                               */
/* ============================================================= */

void Bootloader_ProcessFrame(uint8_t *data, uint16_t len)
{
    
    uint8_t funcCode = data[0];

    if (funcCode == BOOT_PROTOCOL_FUNCTION_CODE) {

        uint8_t subFuncCode = data[1];

        switch (subFuncCode) {

        case SUB_FUNCTION_LOAD_START:
            ProcessLoadStart(data, len);
            break;

        case SUB_FUNCTION_LOAD_DATA:
            ProcessLoadData(data, len);
            break;

        case SUB_FUNCTION_LOAD_COMPLETE:
            ProcessLoadComplete(data, len);
            break;

        case SUB_FUNCTION_LOAD_ACTIVE:
            ProcessLoadActivate(data, len);
            break;

        default:
            SendErrorResponse(ERROR_INVALID_SUB_FUNCTION);
            break;
        }
    } else {

        SendErrorResponse(ERROR_INVALID_FUNCTION_CODE);
    }
}

/* ============================================================= */

static bool IsFrameComplete(uint8_t *buf, uint16_t len)
{
    if (len < 3)
        return false;

    uint8_t dataLen = buf[2];

    uint16_t expectedLen = 3 + dataLen;

    return (len >= expectedLen);
}

static void ProcessReceivedFrame(void)
{
    uint16_t len = frameLen;

    if (IsFrameComplete(frameBuf, len)) {

        uint8_t dataLen = frameBuf[2];

        uint16_t totalLen = 3 + dataLen;

        Bootloader_ProcessFrame(frameBuf, totalLen);

        if (rxIndex > totalLen) {

            memmove(rxBuffer, frameBuf + totalLen, rxIndex - totalLen);

            rxIndex -= totalLen;
        } else {

            rxIndex = 0;
        }
    }
}

/* ============================================================= */
/* CRC Calculation                                                */
/* ============================================================= */

static void UpdateCRC(uint8_t *data, uint16_t length)
{
    currentFile.calculatedCRC = CRC16_Modbus_Continue(currentFile.calculatedCRC, data, length);
}

/* ============================================================= */
/* LOAD START                                                     */
/* ============================================================= */

static bool ValidateLoadStart(const LoadStartReq_t *req)
{
    if (req->functionCode != BOOT_PROTOCOL_FUNCTION_CODE || req->subFunctionCode != SUB_FUNCTION_LOAD_START)
        return false;

    if (req->dataLength != 0x06)
        return false;

    if (req->fileType < FILE_TYPE_BIOS || req->fileType > FILE_TYPE_TOTAL_PACKAGE)
        return false;

    uint32_t fileLength = Convert_BE32(req->fileLength);

    if (fileLength == 0 || fileLength > 0x100000)
        return false;

    return true;
}

/* ============================================================= */

static void ProcessLoadStart(uint8_t *data, uint16_t len)
{
    if (len != sizeof(LoadStartReq_t)) {
        SendErrorResponse(ERROR_INVALID_DATA_LENGTH);
        return;
    }

    LoadStartReq_t *req = (LoadStartReq_t *)data;

    if (!ValidateLoadStart(req)) {
        SendErrorResponse(ERROR_INVALID_FUNCTION_CODE);
        return;
    }

    if (bootState != BOOT_STATE_IDLE && bootState != BOOT_STATE_ERROR) {
        SendErrorResponse(ERROR_INVALID_SEQ_NUM);
        return;
    }

    uint32_t fileLength = Convert_BE32(req->fileLength);

    currentFile.type = (FileType_t)req->fileType;

    currentFile.length = fileLength;

    currentFile.frameSize = (req->frameLength > 224) ? 224 : req->frameLength;

    currentFile.flashAddress = APP_ADDRESS;

    currentFile.calculatedCRC = 0xFFFF;

    currentFile.received = 0;

    currentFile.expectedCRC = 0;

    uint32_t pages = (currentFile.length + FLASH_PAGE_SIZE_BOOT - 1) / FLASH_PAGE_SIZE_BOOT;

    if (!Flash_Erase(currentFile.flashAddress, pages)) {

        SendErrorResponse(ERROR_FLASH_ERASE_FAIL);
        return;
    }

    LoadStartResp_t resp = {

        .functionCode = BOOT_PROTOCOL_FUNCTION_CODE,

        .subFunctionCode = SUB_FUNCTION_LOAD_START,

        .dataLength = 0x06,

        .fileType = req->fileType,

        .fileLength = req->fileLength,

        .frameLength = currentFile.frameSize};

    SendResponse((uint8_t *)&resp, sizeof(resp));

    bootState = BOOT_STATE_LOAD_STARTED;
}

/* ============================================================= */
/* LOAD DATA                                                      */
/* ============================================================= */

static bool ValidateLoadData(const LoadDataReqHeader_t *header, uint16_t dataLen)
{
    if (header->functionCode != BOOT_PROTOCOL_FUNCTION_CODE || header->subFunctionCode != SUB_FUNCTION_LOAD_DATA)
        return false;

    if (header->fileType != currentFile.type)
        return false;

    if (dataLen > currentFile.frameSize)
        return false;

    if (currentFile.received + dataLen > currentFile.length)
        return false;

    return true;
}

/* ============================================================= */

static void ProcessLoadData(uint8_t *data, uint16_t len)
{
    if (len < sizeof(LoadDataReqHeader_t)) {
        SendErrorResponse(ERROR_INVALID_DATA_LENGTH);
        return;
    }

    LoadDataReqHeader_t *header = (LoadDataReqHeader_t *)data;

    uint8_t *frameData = data + sizeof(LoadDataReqHeader_t);

    uint16_t dataLen = header->dataLength - 4;

    if (bootState != BOOT_STATE_LOAD_STARTED && bootState != BOOT_STATE_LOADING_DATA) {

        SendErrorResponse(ERROR_INVALID_SEQ_NUM);

        return;
    }

    if (!ValidateLoadData(header, dataLen)) {

        SendErrorResponse(ERROR_INVALID_DATA_LENGTH);

        return;
    }

    uint32_t address = currentFile.flashAddress + currentFile.received;

    if (!Flash_Write(address, frameData, dataLen)) {

        SendErrorResponse(ERROR_FLASH_WRITE_FAIL);

        return;
    }

    UpdateCRC(frameData, dataLen);

    currentFile.received += dataLen;

    LoadDataResp_t resp = {

        .functionCode = BOOT_PROTOCOL_FUNCTION_CODE,

        .subFunctionCode = SUB_FUNCTION_LOAD_DATA,

        .dataLength = 0x04,

        .fileType = header->fileType,

        .frameSeq = (header->frameSeqLo << 8) | header->frameSeqHi,

        .frameLength = dataLen};

    SendResponse((uint8_t *)&resp, sizeof(resp));

    if (currentFile.received >= currentFile.length)

        bootState = BOOT_STATE_LOAD_COMPLETED;

    else

        bootState = BOOT_STATE_LOADING_DATA;
}

/* ============================================================= */
/* LOAD COMPLETE                                                  */
/* ============================================================= */

static bool ValidateLoadComplete(const LoadCompleteReq_t *req)
{
    if (req->functionCode != BOOT_PROTOCOL_FUNCTION_CODE || req->subFunctionCode != SUB_FUNCTION_LOAD_COMPLETE)
        return false;

    if (req->dataLength != 0x03)
        return false;

    if (req->fileType != currentFile.type)
        return false;

    return true;
}

/* ============================================================= */

static void ProcessLoadComplete(uint8_t *data, uint16_t len)
{
    if (len != sizeof(LoadCompleteReq_t)) {
        SendErrorResponse(ERROR_INVALID_DATA_LENGTH);
        return;
    }

    LoadCompleteReq_t *req = (LoadCompleteReq_t *)data;

    if (bootState != BOOT_STATE_LOAD_COMPLETED) {
        SendErrorResponse(ERROR_INVALID_SEQ_NUM);
        return;
    }

    if (!ValidateLoadComplete(req)) {
        SendErrorResponse(ERROR_INVALID_FUNCTION_CODE);
        return;
    }

    if (currentFile.received != currentFile.length) {
        SendErrorResponse(ERROR_INVALID_DATA_LENGTH);
        return;
    }

    /* Convert received CRC from big-endian to little-endian */

    uint16_t receivedCrc = (req->crcChecksum >> 8) | ((req->crcChecksum & 0xFF) << 8);

    if (currentFile.calculatedCRC != receivedCrc) {

        SendErrorResponse(ERROR_CRC_CHECK_FAIL);
        return;
    }

    currentFile.expectedCRC = req->crcChecksum;

    LoadCompleteResp_t resp = {

        .functionCode = BOOT_PROTOCOL_FUNCTION_CODE,

        .subFunctionCode = SUB_FUNCTION_LOAD_COMPLETE,

        .dataLength = 0x03,

        .fileType = req->fileType,

        .crcChecksum = req->crcChecksum};

    SendResponse((uint8_t *)&resp, sizeof(resp));

    bootState = BOOT_STATE_LOAD_COMPLETED;
}

/* ============================================================= */
/* LOAD ACTIVATE                                                  */
/* ============================================================= */

static void ProcessLoadActivate(uint8_t *data, uint16_t len)
{
    if (len != sizeof(LoadActivateReq_t))
        return;

    LoadActivateReq_t *req = (LoadActivateReq_t *)data;

    if (req->fileType != currentFile.type)
        return;

    if (bootState != BOOT_STATE_LOAD_COMPLETED)
        return;

    if (!Verify_Application(currentFile.flashAddress))
        return;

    RTC_ClearBootInfo();

    HAL_Delay(10);

    JumpTo_Application(currentFile.flashAddress);
}

/* ============================================================= */

BootState_t Bootloader_GetState(void)
{
    return bootState;
}

void Bootloader_SetState(BootState_t state)
{
    bootState = state;
}