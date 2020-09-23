//
// ble_rcs_server.h
//
// Reduced Characteristic Set for BLE
// Code to be run at server (charger device)
//
// Created by David Crespo (VirtualCode) on 2020/09/22
//

#include "ble_rcs.h"
#include "ble_rcs_server.h"

#include <string.h>

// characteristic selector https://open.spotify.com/track/04hWYuhqETLXrUy7S8Rxzp?si=iMGb3JzTRna3ikopCm89Pw
static uint8_t chr_selector = 0;

// buffer for receiving values from downstream value
static uint8_t chr_selectable_buffer[RCS_NUM_IDX * RCS_CHR_OMNIBUS_SIZE];

void rcs_server_set_selector(uint8_t* data, uint16_t len)
{
    if (len < 1) {
        chr_selector = 0;
        return;
    }

    chr_selector = data[0];
}

uint8_t rcs_server_get_selector()
{
    return chr_selector;
}

uint8_t* rcs_server_get_selected_data()
{
    return chr_selectable_buffer + RCS_CHR_OMNIBUS_SIZE * chr_selector;
}

void rcs_server_set_chr_value(uint16_t handle, uint8_t* data, uint16_t len)
{
    uint8_t idx = rcs_idx_for_handle(handle);
    if (len > RCS_CHR_OMNIBUS_SIZE)
        len = RCS_CHR_OMNIBUS_SIZE;
    
    uint8_t* dst = chr_selectable_buffer + RCS_CHR_OMNIBUS_SIZE * idx;
    memset(dst, 0, RCS_CHR_OMNIBUS_SIZE);
    memcpy(dst, data, len);
}

void rcs_server_notify_chr_value(uint16_t handle, uint8_t* data, uint16_t len)
{

}

