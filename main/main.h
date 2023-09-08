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


#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_INOUT_DESC_LEN)

enum
{
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_HID,
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
    STRID_HID_INTERFACE,
    STRID_MSC_INTERFACE
};

enum
{
    EDPT_CTRL_OUT = 0x00,
    EDPT_CTRL_IN = 0x80,
    EDPT_CDC_NOTIFY = 0x81,
    EDPT_CDC_OUT = 0x02,
    EDPT_CDC_IN = 0x82,
    EDPT_HID_OUT = 0x03,
    EDPT_HID_IN = 0x83,
    EDPT_MSC_OUT = 0x04,
    EDPT_MSC_IN = 0x84,
};