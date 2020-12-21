 
#ifndef OTA_Firmware_Upgrade_h 
#define OTA_Firmware_Upgrade_h 

//Includes
#include <inttypes.h>
#include "serverble.h"
#include "esp_ota_ops.h"
#include "HardwareSerialMOD.h"
#include "SPIFFS.h"

//Defines
#define FULL_PACKET 512
#define CHARPOS_UPDATE_FLAG 5


/*******************************************************************************
* Bootloader packet byte addresses:
* [1-byte] [1-byte ] [2-byte] [n-byte] [ 2-byte ] [1-byte]
* [ SOP  ] [Command] [ Size ] [ Data ] [Checksum] [ EOP  ]
*******************************************************************************/
#define BootloaderEmulator_SOP_ADDR             (0x00u)         /* Start of packet offset from beginning     */
#define BootloaderEmulator_CMD_ADDR             (0x01u)         /* Command offset from beginning             */
#define BootloaderEmulator_SIZE_ADDR            (0x02u)         /* Packet size offset from beginning         */
#define BootloaderEmulator_DATA_ADDR            (0x04u)         /* Packet data offset from beginning         */
#define BootloaderEmulator_CHK_ADDR(x)          (0x04u + (x))   /* Packet checksum offset from end           */
#define BootloaderEmulator_EOP_ADDR(x)          (0x06u + (x))   /* End of packet offset from end             */
#define BootloaderEmulator_MIN_PKT_SIZE         (7u)            /* The minimum number of bytes in a packet   */


#define CMD_START               0x01
#define CMD_STOP                0x17
#define BASE_CMD_SIZE           0x07
#define CMD_VERIFY_CHECKSUM     0x31
#define CMD_GET_FLASH_SIZE      0x32
#define CMD_GET_APP_STATUS      0x33
#define CMD_ERASE_ROW           0x34
#define CMD_SYNC                0x35
#define CMD_SET_ACTIVE_APP      0x36
#define CMD_SEND_DATA           0x37
#define CMD_ENTER_BOOTLOADER    0x38
#define CMD_PROGRAM_ROW         0x39
#define CMD_VERIFY_ROW          0x3A
#define CMD_EXIT_BOOTLOADER     0x3B

/* Bootloader command definitions. */
#define BootloaderEmulator_COMMAND_CHECKSUM     (0x31u)    /* Verify the checksum for the bootloadable project   */
#define BootloaderEmulator_COMMAND_REPORT_SIZE  (0x32u)    /* Report the programmable portions of flash          */
#define BootloaderEmulator_COMMAND_APP_STATUS   (0x33u)    /* Gets status info about the provided app status     */
#define BootloaderEmulator_COMMAND_ERASE        (0x34u)    /* Erase the specified flash row                      */
#define BootloaderEmulator_COMMAND_SYNC         (0x35u)    /* Sync the bootloader and host application           */
#define BootloaderEmulator_COMMAND_APP_ACTIVE   (0x36u)    /* Sets the active application                        */
#define BootloaderEmulator_COMMAND_DATA         (0x37u)    /* Queue up a block of data for programming           */
#define BootloaderEmulator_COMMAND_ENTER        (0x38u)    /* Enter the bootloader                               */
#define BootloaderEmulator_COMMAND_PROGRAM      (0x39u)    /* Program the specified row                          */
#define BootloaderEmulator_COMMAND_VERIFY       (0x3Au)    /* Compute flash row checksum for verification        */
#define BootloaderEmulator_COMMAND_EXIT         (0x3Bu)    /* Exits the bootloader & resets the chip             */
#define BootloaderEmulator_COMMAND_GET_METADATA (0x3Cu)    /* Reports the metadata for a selected application    */

/*Direcciones del archivo*/
#define PRIMERA_FILA 14
#define TAMANO_FILA  527



//Prototipos de funciones
void WriteDataToOta(BLECharacteristic *pCharacteristic);
void StartPsocUpdate(HardwareSerialMOD serialLocal);
#endif