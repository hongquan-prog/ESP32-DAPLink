/*
 * Copyright (c) 2026-2026, hongquan.li
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_err.h"
#include "nvs_flash.h"

#include "usbip_util.h"

#define USBIP_DESC_NVS_NAMESPACE "usbip"
#define USBIP_DESC_NVS_KEY "serial_number"

int usbip_desc_get_serial_ascii(char* serial, uint8_t serial_size)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t required_len = 0;

    if (!serial || serial_size < 2)
    {
        return 0;
    }

    err = nvs_open(USBIP_DESC_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        return 0;
    }

    err = nvs_get_str(nvs_handle, USBIP_DESC_NVS_KEY, NULL, &required_len);
    if (err != ESP_OK || required_len <= 1)
    {
        nvs_close(nvs_handle);
        return 0;
    }

    if (required_len > serial_size)
    {
        required_len = serial_size;
    }

    err = nvs_get_str(nvs_handle, USBIP_DESC_NVS_KEY, serial, &required_len);
    nvs_close(nvs_handle);
    if (err != ESP_OK)
    {
        return 0;
    }

    serial[serial_size - 1] = '\0';
    if (serial[0] == '\0')
    {
        return 0;
    }

    return 1;
}