/*
 * Copyright (c) 2026-2026, hongquan.li
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-3-24      hongquan.li   add license declaration
 */

#ifndef USB_CONTROL_H
#define USB_CONTROL_H

#include <stddef.h>
#include <stdint.h>
#include "usbip_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * USB Standard Control Transfer Framework
 *
 * Provides reusable control transfer handling logic to reduce code duplication
 *****************************************************************************/

/*****************************************************************************
 * Return Values
 *****************************************************************************/

#define USB_CONTROL_OK 0         /* Success, data returned */
#define USB_CONTROL_OK_NO_DATA 1 /* Success, no data */
#define USB_CONTROL_STALL 2      /* Not supported, return STALL */
#define USB_CONTROL_ERROR -1     /* Internal error */

/**
 * Initialize control transfer context (static initialization)
 */
#define USB_CONTROL_CONTEXT_INIT(dev_desc, cfg_desc, cfg_len)                                      \
    {                                                                                              \
        .device_desc = (dev_desc),                                                                 \
        .config_desc = (cfg_desc),                                                                 \
        .config_desc_len = (cfg_len),                                                              \
        .num_configs = 1,                                                                          \
        .address = 0,                                                                              \
        .config_value = 0,                                                                         \
    }


/*****************************************************************************
 * Configuration
 *****************************************************************************/

#define USB_MAX_INTERFACES 4

/*****************************************************************************
 * Callback Types
 *****************************************************************************/

/**
 * Descriptor callback - Handle non-standard descriptor requests
 */
typedef int (*usb_descriptor_handler)(uint8_t type, uint8_t index, void* user_data, void** data_out,
                                      size_t* data_len);

/**
 * String descriptor callback
 */
typedef int (*usb_string_handler)(uint8_t index, void* user_data, void** data_out,
                                  size_t* data_len);

/**
 * Configuration change callback
 */
typedef void (*usb_config_handler)(uint8_t config, void* user_data);

/**
 * Class request callback
 */
typedef int (*usb_class_handler)(const struct usb_setup_packet* setup, void* user_data,
                                 void** data_out, size_t* data_len);

/**
 * Vendor request callback
 */
typedef int (*usb_vendor_handler)(const struct usb_setup_packet* setup, void* user_data,
                                  void** data_out, size_t* data_len);

/*****************************************************************************
 * Control Transfer Context
 *****************************************************************************/

struct usb_control_context
{
    /*--- Device Descriptor ---*/
    const struct usb_device_descriptor* device_desc;
    const void* config_desc; /* Configuration descriptor (includes interface and endpoints) */
    size_t config_desc_len;
    uint8_t num_configs;

    /*--- HID Descriptor (optional) ---*/
    const struct usb_hid_descriptor* hid_desc;
    const uint8_t* report_desc;
    size_t report_desc_len;

    /*--- BOS Descriptor (optional, required for USB 2.01+) ---*/
    const void* bos_desc;
    size_t bos_desc_len;

    /*--- String Descriptors ---*/
    const uint8_t* lang_id_desc;  /* Language ID descriptor */
    const uint8_t** string_descs; /* String descriptor array */
    uint8_t num_strings;

    /*--- Device State ---*/
    uint8_t address;
    uint8_t config_value;
    uint8_t alt_setting[USB_MAX_INTERFACES];

    /*--- Callbacks ---*/
    usb_descriptor_handler descriptor_handler;
    usb_string_handler string_handler;
    usb_config_handler config_handler;
    usb_class_handler class_handler;
    usb_vendor_handler vendor_handler;

    /*--- User Data ---*/
    void* user_data;
};

/**
 * usb_control_handle_setup - Handle USB Setup request
 * @setup: Setup packet
 * @ctx: Control transfer context
 * @data_out: Output data buffer pointer (caller must free)
 * @data_len: Output data length
 * Return: USB_CONTROL_* value
 *
 * Caller is responsible for osal_free(*data_out)
 */
int usb_control_handle_setup(const struct usb_setup_packet* setup, struct usb_control_context* ctx,
                             void** data_out, size_t* data_len);

/**
 * usb_control_get_descriptor - Handle GET_DESCRIPTOR request
 * @setup: Setup packet
 * @ctx: Control transfer context
 * @data_out: Output data buffer pointer
 * @data_len: Output data length
 * Return: USB_CONTROL_* value
 */
int usb_control_get_descriptor(const struct usb_setup_packet* setup,
                               struct usb_control_context* ctx, void** data_out, size_t* data_len);

/**
 * usb_control_get_string - Handle string descriptor request
 * @index: String index
 * @ctx: Control transfer context
 * @data_out: Output data buffer pointer
 * @data_len: Output data length
 * Return: USB_CONTROL_* value
 */
int usb_control_get_string(uint8_t index, struct usb_control_context* ctx, void** data_out,
                           size_t* data_len);

/**
 * usb_control_set_config - Handle SET_CONFIGURATION request
 * @setup: Setup packet
 * @ctx: Control transfer context
 * @data_out: Output data buffer pointer
 * @data_len: Output data length
 * Return: USB_CONTROL_* value
 */
int usb_control_set_config(const struct usb_setup_packet* setup, struct usb_control_context* ctx,
                           void** data_out, size_t* data_len);

/**
 * usb_control_get_config - Handle GET_CONFIGURATION request
 * @setup: Setup packet
 * @ctx: Control transfer context
 * @data_out: Output data buffer pointer
 * @data_len: Output data length
 * Return: USB_CONTROL_* value
 */
int usb_control_get_config(const struct usb_setup_packet* setup, struct usb_control_context* ctx,
                           void** data_out, size_t* data_len);

/**
 * usb_control_get_status - Handle GET_STATUS request
 * @setup: Setup packet
 * @ctx: Control transfer context
 * @data_out: Output data buffer pointer
 * @data_len: Output data length
 * Return: USB_CONTROL_* value
 */
int usb_control_get_status(const struct usb_setup_packet* setup, struct usb_control_context* ctx,
                           void** data_out, size_t* data_len);

/**
 * usb_control_class_request - Handle class-specific request
 * @setup: Setup packet
 * @ctx: Control transfer context
 * @data_out: Output data buffer pointer
 * @data_len: Output data length
 * Return: USB_CONTROL_* value
 */
int usb_control_class_request(const struct usb_setup_packet* setup, struct usb_control_context* ctx,
                              void** data_out, size_t* data_len);

#ifdef __cplusplus
}
#endif

#endif /* USB_CONTROL_H */