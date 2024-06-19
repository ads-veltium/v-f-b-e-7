/*
 * Copyright 2011-2023 Cypress Semiconductor Corporation (an Infineon company)
 * SPDX-License-Identifier: Apache-2.0
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cybtldr_api.h"
#include "../control.h"
#include "../helpers.h"

HardwareSerialMOD* channel = NULL;

static uint16_t min_uint16(uint16_t a, uint16_t b) { return (a < b) ? a : b; }

static void writePacketToFile(uint8_t* inBuf, uint32_t inSize, FILE* outFile) {
    for (uint32_t i = 0; i < inSize; i++) {
                    fprintf(outFile, "%02x ", inBuf[i]);
    }
    fprintf(outFile, "\n");
}

int CyBtldr_TransferData(uint8_t* inBuf, int inSize, uint8_t* outBuf, int outSize) {
    int err = 1;
    uint8 cnt_timeout_tx = 1;
    int timeout = 0;

    outBuf[0] = 'a';

    channel->flush(false);

    while (err == 1)
    {
        if (timeout > 4000)
        {
            Serial.println(timeout);
            err = 68;
        }
    
        if (--cnt_timeout_tx == 0)
        {
            cnt_timeout_tx = 25;
            sendBinaryBlock(inBuf, inSize);
            vTaskDelay(pdMS_TO_TICKS(10));
            timeout++;
        }
        if (channel->available() != 0)
        {
#ifdef DEBUG_UPDATE
            Serial.print("Available: ");
            Serial.println(channel->available());
            Serial.println(outSize);
#endif
            int longitud_bloque = outSize;
            int puntero_rx_local = 0;
            do
            {
                if (longitud_bloque > 0)
                {
                    channel->read(&outBuf[puntero_rx_local], 1);
                    puntero_rx_local++;
                }
                longitud_bloque--;
            } while (longitud_bloque > 0);
            for (int i = 0; i < outSize; i++)
            {
#ifdef DEBUG_UPDATE
                Serial.print(outBuf[i]);
                Serial.print(" ");
#endif
            }
            if (outBuf[0] == 1 && outBuf[outSize - 1] == 0x17 && outBuf[1] == 0)
            {
#ifdef DEBUG_UPDATE
                Serial.print("outBuf[1]: ");
                Serial.println(outBuf[1]);
#endif
                err = outBuf[1];
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    channel->flush();
    if (CYRET_SUCCESS != err)
        err |= CYRET_ERR_COMM_MASK;

    return err;
}

int CyBtldr_ReadData(uint8_t* outBuf, int outSize) {
    int err = 0; //g_comm->ReadData(outBuf, outSize);

    if (CYRET_SUCCESS != err) err |= CYRET_ERR_COMM_MASK;

    return err;
}

int CyBtldr_StartBootloadOperation(HardwareSerialMOD *ReceivedChannel, uint32_t expSiId, uint8_t expSiRev, uint32_t *blVer,
                                   uint32_t productID)
{
    uint32_t inSize = 0;
    uint32_t outSize = 0;
    uint32_t siliconId = 0;
    uint8_t inBuf[MAX_COMMAND_SIZE];
    uint8_t outBuf[MAX_COMMAND_SIZE];
    uint8_t siliconRev = 0;
    uint8_t status = CYRET_SUCCESS;
    int err = CYRET_SUCCESS;

    channel = ReceivedChannel;

    if (CYRET_SUCCESS == err) {
        err = CyBtldr_CreateEnterBootLoaderCmd(inBuf, &inSize, &outSize, productID);
        if (CYRET_SUCCESS == err) {
            err = CyBtldr_TransferData(inBuf, inSize, outBuf, outSize);
            if (CYRET_SUCCESS == err)
                err = CyBtldr_ParseEnterBootLoaderCmdResult(outBuf, outSize, &siliconId, &siliconRev, blVer, &status);
            else if (CyBtldr_TryParseParketStatus(outBuf, outSize, &status) == CYRET_SUCCESS)
                err = status | CYRET_ERR_BTLDR_MASK; // if the response we get back is a valid packet override the err with the response's status
        }
    }
    if (CYRET_SUCCESS == err) {
        if (CYRET_SUCCESS != status)
            err = status | CYRET_ERR_BTLDR_MASK;
        if (expSiId != siliconId || expSiRev != siliconRev)
            err = CYRET_ERR_DEVICE;
    }

    return err;
}

int CyBtldr_EndBootloadOperation(HardwareSerialMOD *ReceivedChannel)
{
    uint32_t inSize;
    uint32_t outSize;
    uint8_t inBuf[MAX_COMMAND_SIZE];

    int err = CyBtldr_CreateExitBootLoaderCmd(inBuf, &inSize, &outSize);
    if (CYRET_SUCCESS == err) {
        err = ReceivedChannel->write(inBuf, inSize);

        if (CYRET_SUCCESS != err) err |= CYRET_ERR_COMM_MASK;
    }
    channel = NULL;

    return err;
}

static int SendData(HardwareSerialMOD *ReceivedChannel, uint8_t *buf, uint16_t size, uint16_t *offset, uint16_t maxRemainingDataSize, uint8_t *inBuf, uint8_t *outBuf, FILE *outFile)
{
    uint8_t status = CYRET_SUCCESS;
    uint32_t inSize = 0, outSize = 0;
    // size is the total bytes of data to transfer.
    // offset is the amount of data already transfered.
    // a is maximum amount of data allowed to be left over when this function ends.
    // (we can leave some data for caller (programRow, VerifyRow,...) to send.
    // TRANSFER_HEADER_SIZE is the amount of bytes this command header takes up.
    const uint16_t TRANSFER_HEADER_SIZE = 7;
    uint16_t subBufSize = min_uint16((uint16_t)(128 - TRANSFER_HEADER_SIZE), size);
    int err = CYRET_SUCCESS;
    uint16_t cmdLen = 0;
    // Break row into pieces to ensure we don't send too much for the transfer protocol
    while ((CYRET_SUCCESS == err) && ((size - (*offset)) > maxRemainingDataSize)) {
        if ((size - (*offset)) > subBufSize) {
            cmdLen = subBufSize;
        } else {
            cmdLen = size - (*offset);
        }
        err = CyBtldr_CreateSendDataCmd(&buf[*offset], cmdLen, inBuf, &inSize, &outSize);
        if (CYRET_SUCCESS == err) {
#ifdef DEBUG_UPDATE
            Serial.println("CyBtldr_CreateSendDataCmd success");
#endif
            if (outFile != NULL)
            {
                fprintf(outFile, "Write packet: ");
                writePacketToFile(inBuf, inSize, outFile);
            }
            err = CyBtldr_TransferData(inBuf, inSize, outBuf, outSize);
#ifdef DEBUG_UPDATE
            Serial.print("SendData CyBtldr_TransferData ");
            Serial.println(err);
#endif
            if (CYRET_SUCCESS == err)
            {
                if (outFile != NULL) {
                    fprintf(outFile, " Read packet: ");
                    writePacketToFile(outBuf, outSize, outFile);
                }
                err = CyBtldr_ParseSendDataCmdResult(outBuf, outSize, &status);
#ifdef DEBUG_UPDATE
                Serial.print("CyBtldr_ParseSendDataCmdResult ");
                Serial.println(err);
#endif               
            }
        }
        if (CYRET_SUCCESS != status) err = status | CYRET_ERR_BTLDR_MASK;

        (*offset) += cmdLen;
    }
    return err;
}

int CyBtldr_ProgramRow(uint32_t address, uint8_t* buf, uint16_t size) {
    const size_t TRANSFER_HEADER_SIZE = 15;

    uint8_t inBuf[MAX_COMMAND_SIZE];
    uint8_t outBuf[MAX_COMMAND_SIZE];
    uint32_t inSize;
    uint32_t outSize;
    uint16_t offset = 0;
    uint16_t subBufSize;
    uint8_t status = CYRET_SUCCESS;
    int err = CYRET_SUCCESS;

    uint32_t chksum = CyBtldr_ComputeChecksum32bit(buf, size);

    uint16_t maxDataTransferSize = (128 >= TRANSFER_HEADER_SIZE)
                        ? (uint16_t)(128 - TRANSFER_HEADER_SIZE)
                        : 0;
        
    if (CYRET_SUCCESS == err)
        err = SendData(channel, buf, size, &offset, maxDataTransferSize, inBuf, outBuf, NULL);

    if (CYRET_SUCCESS == err) {
        subBufSize = (uint16_t)(size - offset);
        err = CyBtldr_CreateProgramDataCmd(address, chksum, &buf[offset], subBufSize, inBuf, &inSize, &outSize);
        if (CYRET_SUCCESS == err) {
            err = CyBtldr_TransferData(inBuf, inSize, outBuf, outSize);
            if (CYRET_SUCCESS == err) {
                err = CyBtldr_ParseDefaultCmdResult(outBuf, outSize, &status);
            }
        }
        if (CYRET_SUCCESS != status) err = status | CYRET_ERR_BTLDR_MASK;
    }

    return err;
}

int CyBtldr_EraseRow(uint32_t address) {
    uint8_t inBuf[MAX_COMMAND_SIZE];
    uint8_t outBuf[MAX_COMMAND_SIZE];
    uint32_t inSize = 0;
    uint32_t outSize = 0;
    uint8_t status = CYRET_SUCCESS;
    int err = CYRET_SUCCESS;

    if (CYRET_SUCCESS == err) {
        err = CyBtldr_CreateEraseDataCmd(address, inBuf, &inSize, &outSize);
        if (CYRET_SUCCESS == err) {
            err = CyBtldr_TransferData(inBuf, inSize, outBuf, outSize);
            if (CYRET_SUCCESS == err) {
                err = CyBtldr_ParseDefaultCmdResult(outBuf, outSize, &status);
            }
        }
    }
    if (CYRET_SUCCESS != status) err = status | CYRET_ERR_BTLDR_MASK;

    return err;
}

int CyBtldr_VerifyRow(uint32_t address, uint8_t* buf, uint16_t size) {
    const size_t TRANSFER_HEADER_SIZE = 15;

    uint8_t inBuf[MAX_COMMAND_SIZE];
    uint8_t outBuf[MAX_COMMAND_SIZE];
    uint32_t inSize;
    uint32_t outSize;
    uint16_t offset = 0;
    uint16_t subBufSize;
    uint8_t status = CYRET_SUCCESS;
    int err = CYRET_SUCCESS;

    uint32_t chksum = CyBtldr_ComputeChecksum32bit(buf, size);

    uint16_t maxDataTransferSize = (128 >= TRANSFER_HEADER_SIZE)
                        ? (uint16_t)(128 - TRANSFER_HEADER_SIZE)
                        : 0;
    if (CYRET_SUCCESS == err && NULL != channel)
        err = SendData(channel, buf, size, &offset, maxDataTransferSize, inBuf, outBuf, NULL);

    if (CYRET_SUCCESS == err) {
        subBufSize = (uint16_t)(size - offset);

        err = CyBtldr_CreateVerifyDataCmd(address, chksum, &buf[offset], subBufSize, inBuf, &inSize, &outSize);
        if (CYRET_SUCCESS == err) {
            err = CyBtldr_TransferData(inBuf, inSize, outBuf, outSize);
            if (CYRET_SUCCESS == err) {
                err = CyBtldr_ParseDefaultCmdResult(outBuf, outSize, &status);
            }
        }
        if (CYRET_SUCCESS != status)
        {
            err = status | CYRET_ERR_BTLDR_MASK;
            if (CYRET_ERR_CHECKSUM == err)
            {
#ifdef DEBUG_UPDATE
                Serial.println("Checksum error detected");
                Serial.println(address);
                Serial.println(chksum);
#endif
            }
        }
    }

    return err;
}

int CyBtldr_VerifyApplication(uint8_t appId) {
    uint8_t inBuf[MAX_COMMAND_SIZE];
    uint8_t outBuf[MAX_COMMAND_SIZE];
    uint32_t inSize = 0;
    uint32_t outSize = 0;
    uint8_t checksumValid = 0;
    uint8_t status = CYRET_SUCCESS;

    int err = CyBtldr_CreateVerifyChecksumCmd(appId, inBuf, &inSize, &outSize);
    if (CYRET_SUCCESS == err) {
        err = CyBtldr_TransferData(inBuf, inSize, outBuf, outSize);
        if (CYRET_SUCCESS == err) {
            err = CyBtldr_ParseVerifyChecksumCmdResult(outBuf, outSize, &checksumValid, &status);
        }
    }
    if (CYRET_SUCCESS != status) err = status | CYRET_ERR_BTLDR_MASK;
    if ((CYRET_SUCCESS == err) && (!checksumValid)) err = CYRET_ERR_CHECKSUM;

    return err;
}

int CyBtldr_SetApplicationMetaData(uint8_t appId, uint32_t appStartAddr, uint32_t appSize) {
    uint8_t inBuf[MAX_COMMAND_SIZE];
    uint8_t outBuf[MAX_COMMAND_SIZE];
    uint32_t inSize = 0;
    uint32_t outSize = 0;
    uint8_t status = CYRET_SUCCESS;

    uint8_t metadata[8];
    metadata[0] = (uint8_t)appStartAddr;
    metadata[1] = (uint8_t)(appStartAddr >> 8);
    metadata[2] = (uint8_t)(appStartAddr >> 16);
    metadata[3] = (uint8_t)(appStartAddr >> 24);
    metadata[4] = (uint8_t)appSize;
    metadata[5] = (uint8_t)(appSize >> 8);
    metadata[6] = (uint8_t)(appSize >> 16);
    metadata[7] = (uint8_t)(appSize >> 24);
    int err = CyBtldr_CreateSetApplicationMetadataCmd(appId, metadata, inBuf, &inSize, &outSize);
    if (CYRET_SUCCESS == err) {
        err = CyBtldr_TransferData(inBuf, inSize, outBuf, outSize);
        if (CYRET_SUCCESS == err) {
            err = CyBtldr_ParseDefaultCmdResult(outBuf, outSize, &status);
        }
    }
    if (CYRET_SUCCESS != status) err = status | CYRET_ERR_BTLDR_MASK;

    return err;
}

int CyBtldr_SetEncryptionInitialVector(uint16_t size, uint8_t* buf) {
    uint8_t inBuf[MAX_COMMAND_SIZE];
    uint8_t outBuf[MAX_COMMAND_SIZE];

    uint32_t inSize = 0;
    uint32_t outSize = 0;
    uint8_t status = CYRET_SUCCESS;

    int err = CyBtldr_CreateSetEncryptionInitialVectorCmd(buf, size, inBuf, &inSize, &outSize);
    if (CYRET_SUCCESS == err) {
        err = CyBtldr_TransferData(inBuf, inSize, outBuf, outSize);
        if (CYRET_SUCCESS == err) {
            err = CyBtldr_ParseDefaultCmdResult(outBuf, outSize, &status);
        }
    }
    if (CYRET_SUCCESS != status) err = status | CYRET_ERR_BTLDR_MASK;
    return err;
}
