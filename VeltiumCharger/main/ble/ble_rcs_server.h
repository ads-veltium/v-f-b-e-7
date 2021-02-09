//
// ble_rcs_server.h
//
// Reduced Characteristic Set for BLE
// Code to be run at server (charger device)
//
// Created by David Crespo (VirtualCode) on 2020/09/22
//

#ifndef ble_rcs_server_h
#define ble_rcs_server_h

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

uint8_t* rcs_server_get_data_for_selector(uint8_t selector);

void rcs_server_set_chr_value(uint16_t handle, uint8_t* data, uint16_t len);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // ble_rcs_server_h
