//
// ble_rcs.h
//
// Reduced Characteristic Set for BLE
//
// Created by David Crespo (VirtualCode) on 2020/09/22
//

#ifndef ble_rcs_h
#define ble_rcs_h

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define RCS_MAX_IDX 64
#define RCS_NUM_IDX (1+RCS_MAX_IDX)

#define RCS_MAX_HANDLE 0x00AF

#define RCS_CHR_OMNIBUS_SIZE 16

// get characteristic index for given handle
uint8_t rcs_idx_for_handle(uint16_t handle);

// get characteristic handle for given index
uint16_t rcs_handle_for_idx(uint8_t idx);

// get characteristic size for given index
uint8_t rcs_get_size(uint8_t idx);

// bits for R/W/N type mask
#define RCS_RWN_READ   1
#define RCS_RWN_WRITE  2
#define RCS_RWN_NOTIFY 4

// get characteristic R/W/N type mask for given index
uint8_t rcs_get_type(uint8_t idx);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // ble_rcs_h