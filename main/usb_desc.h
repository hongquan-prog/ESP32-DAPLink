/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   add license declaration
 */

#pragma once

#include "tinyusb.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C"
{
#endif

    enum
    {
        ITF_NUM_CDC = 0,
        ITF_NUM_CDC_DATA,
#ifdef CONFIG_ENABLE_WEBUSB
        ITF_NUM_BULK,
#else
        ITF_NUM_HID,
#endif
        ITF_NUM_MSC,
        ITF_NUM_TOTAL
    };

    enum
    {
        STRID_LANGID = 0,
        STRID_MANUFACTURER,
        STRID_PRODUCT,
        STRID_SERIAL_NUMBER,
        STRID_CDC_INTERFACE,
#ifdef CONFIG_ENABLE_WEBUSB
        STRID_BULK_INTERFACE,
#else
        STRID_HID_INTERFACE,
#endif
        STRID_MSC_INTERFACE
    };

    enum
    {
        EDPT_CTRL_OUT = 0x00,
        EDPT_CTRL_IN = 0x80,
        EDPT_CDC_NOTIFY = 0x81,
        EDPT_CDC_OUT = 0x02,
        EDPT_CDC_IN = 0x82,
#ifdef CONFIG_ENABLE_WEBUSB
        EDPT_BULK_OUT = 0x03,
        EDPT_BULK_IN = 0x83,
#else
        EDPT_HID_OUT = 0x03,
        EDPT_HID_IN = 0x83,
#endif
        EDPT_MSC_OUT = 0x04,
        EDPT_MSC_IN = 0x84,
    };

    enum
    {
        VENDOR_REQUEST_WEBUSB = 1,
        VENDOR_REQUEST_MICROSOFT = 2
    };

    tusb_desc_device_t *get_device_descriptor(void);
    const char **get_string_descriptor(bool with_msc);
    int get_string_descriptor_count(bool with_msc);
    const uint8_t *get_configuration_descriptor(bool with_msc);
#ifdef CONFIG_ENABLE_WEBUSB
    uint8_t *get_ms_descriptor(void);
    tusb_desc_webusb_url_t *get_webusb_url(void);
#endif

#ifdef __cplusplus
}
#endif