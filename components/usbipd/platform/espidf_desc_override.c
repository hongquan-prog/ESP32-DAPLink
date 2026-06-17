/*
 * Copyright (c) 2026-2026, hongquan.li
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>

#include "usb_desc.h"


int usbip_desc_get_serial_ascii(char* serial, uint8_t serial_size)
{
    return usb_desc_get_serial_ascii(serial, (size_t)serial_size);
}
