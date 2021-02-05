#include "ble_rcs.h"

#include "ble_rcs_tables.h"

uint8_t rcs_idx_for_handle(uint16_t handle)
{
    if (handle <= RCS_MAX_HANDLE)
        return table_idx_for_handle[handle];
    else
        return 0;
}

uint16_t rcs_handle_for_idx(uint8_t idx)
{
    if (idx <= RCS_MAX_IDX)
        return table_handle_for_idx[idx];
    else
        return 0;
}

uint8_t rcs_get_size(uint8_t idx)
{
    if (idx <= RCS_MAX_IDX)
        return table_size_for_idx[idx];
    else
        return 0;
}

uint8_t rcs_get_type(uint8_t idx)
{
    if (idx <= RCS_MAX_IDX)
        return table_type_for_idx[idx];
    else
        return 0;
}


