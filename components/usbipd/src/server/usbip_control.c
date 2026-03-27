/*
 * Copyright (c) 2026-2026, hongquan.li
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-3-24      hongquan.li   add license declaration
 */

/*****************************************************************************
 * USB Standard Control Transfer Framework
 *
 * Provides a generic framework for standard USB control transfers
 * Can be reused by device drivers to reduce code duplication
 *****************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "hal/usbip_log.h"
#include "hal/usbip_osal.h"
#include "usbip_common.h"
#include "usbip_control.h"

LOG_MODULE_REGISTER(control, CONFIG_USBIP_LOG_LEVEL);

/*****************************************************************************
 * Default String Descriptors
 *****************************************************************************/

static const uint8_t default_string0[] = {
    0x04, USB_DT_STRING, 0x09, 0x04 /* English (US) */
};

/*****************************************************************************
 * Control Transfer Handling
 *****************************************************************************/

int usb_control_handle_setup(const struct usb_setup_packet* setup, struct usb_control_context* ctx,
                             void** data_out, size_t* data_len)
{
    int ret = USB_CONTROL_OK;

    if (!setup || !ctx || !data_out || !data_len)
    {
        return USB_CONTROL_ERROR;
    }

    *data_out = NULL;
    *data_len = 0;

    uint8_t type = USB_SETUP_TYPE(setup);

    /* Standard device requests */
    if (type == USB_TYPE_STANDARD)
    {
        switch (setup->bRequest)
        {
            case USB_REQ_GET_DESCRIPTOR:
                ret = usb_control_get_descriptor(setup, ctx, data_out, data_len);
                break;

            case USB_REQ_SET_CONFIGURATION:
                ret = usb_control_set_config(setup, ctx, data_out, data_len);
                break;

            case USB_REQ_GET_CONFIGURATION:
                ret = usb_control_get_config(setup, ctx, data_out, data_len);
                break;

            case USB_REQ_SET_ADDRESS:
                /* Virtual devices typically ignore address setting */
                ctx->address = setup->wValue & 0xFF;
                ret = USB_CONTROL_OK_NO_DATA;
                break;

            case USB_REQ_GET_STATUS:
                ret = usb_control_get_status(setup, ctx, data_out, data_len);
                break;

            case USB_REQ_SET_FEATURE:
            case USB_REQ_CLEAR_FEATURE:
                /* Virtual devices typically don't support remote wakeup or endpoint halt */
                ret = USB_CONTROL_OK_NO_DATA;
                break;

            case USB_REQ_SET_INTERFACE:
                /* Default support, no actual operation */
                ctx->alt_setting[setup->wIndex & 0xFF] = setup->wValue & 0xFF;
                ret = USB_CONTROL_OK_NO_DATA;
                break;

            case USB_REQ_GET_INTERFACE: {
                uint8_t intf = setup->wIndex & 0xFF;
                uint8_t* p = osal_malloc(1);
                if (p)
                {
                    *p = (intf < USB_MAX_INTERFACES) ? ctx->alt_setting[intf] : 0;
                    *data_out = p;
                    *data_len = 1;
                    ret = USB_CONTROL_OK;
                }
                else
                {
                    ret = USB_CONTROL_ERROR;
                }
            }
            break;

            default:
                ret = USB_CONTROL_STALL;
                break;
        }
    }
    /* Class-specific requests */
    else if (type == USB_TYPE_CLASS)
    {
        ret = usb_control_class_request(setup, ctx, data_out, data_len);
    }
    /* Vendor requests */
    else if (type == USB_TYPE_VENDOR)
    {
        if (ctx->vendor_handler)
        {
            ret = ctx->vendor_handler(setup, ctx->user_data, data_out, data_len);
        }
        else
        {
            ret = USB_CONTROL_STALL;
        }
    }
    else
    {
        ret = USB_CONTROL_STALL;
    }

    /* Limit returned data length */
    if (*data_len > setup->wLength && *data_out)
    {
        *data_len = setup->wLength;
    }

    return ret;
}

/*****************************************************************************
 * GET_DESCRIPTOR Handling
 *****************************************************************************/

int usb_control_get_descriptor(const struct usb_setup_packet* setup,
                               struct usb_control_context* ctx, void** data_out, size_t* data_len)
{
    uint8_t desc_type = (setup->wValue >> 8) & 0xFF;
    uint8_t desc_index = setup->wValue & 0xFF;

    LOG_INF("GET_DESCRIPTOR: type=0x%02x index=%d wLength=%d", desc_type, desc_index,
            setup->wLength);

    switch (desc_type)
    {
        case USB_DT_DEVICE:
            if (ctx->device_desc)
            {
                *data_out = osal_malloc(USB_DT_DEVICE_SIZE);
                if (*data_out)
                {
                    memcpy(*data_out, ctx->device_desc, USB_DT_DEVICE_SIZE);
                    *data_len = USB_DT_DEVICE_SIZE;
                    return USB_CONTROL_OK;
                }
            }
            break;

        case USB_DT_CONFIG:
            if (ctx->config_desc && ctx->config_desc_len > 0)
            {
                if (desc_index < ctx->num_configs)
                {
                    *data_out = osal_malloc(ctx->config_desc_len);
                    if (*data_out)
                    {
                        memcpy(*data_out, ctx->config_desc, ctx->config_desc_len);
                        *data_len = ctx->config_desc_len;
                        return USB_CONTROL_OK;
                    }
                }
            }
            break;

        case USB_DT_STRING:
            return usb_control_get_string(desc_index, ctx, data_out, data_len);

        case USB_DT_DEVICE_QUALIFIER:
            /* USB 2.0 full-speed devices return STALL */
            return USB_CONTROL_STALL;

        case USB_DT_OTHER_SPEED_CONFIG:
            return USB_CONTROL_STALL;

        case USB_DT_HID:
            if (ctx->hid_desc)
            {
                *data_out = osal_malloc(USB_DT_HID_SIZE);
                if (*data_out)
                {
                    memcpy(*data_out, ctx->hid_desc, USB_DT_HID_SIZE);
                    *data_len = USB_DT_HID_SIZE;
                    return USB_CONTROL_OK;
                }
            }
            break;

        case USB_DT_REPORT:
            if (ctx->report_desc && ctx->report_desc_len > 0)
            {
                *data_out = osal_malloc(ctx->report_desc_len);
                if (*data_out)
                {
                    memcpy(*data_out, ctx->report_desc, ctx->report_desc_len);
                    *data_len = ctx->report_desc_len;
                    return USB_CONTROL_OK;
                }
            }
            break;

        case USB_DT_BOS:
            if (ctx->bos_desc && ctx->bos_desc_len > 0)
            {
                *data_out = osal_malloc(ctx->bos_desc_len);
                if (*data_out)
                {
                    memcpy(*data_out, ctx->bos_desc, ctx->bos_desc_len);
                    *data_len = ctx->bos_desc_len;
                    return USB_CONTROL_OK;
                }
            }
            break;

        default:
            /* Try callback */
            if (ctx->descriptor_handler)
            {
                return ctx->descriptor_handler(desc_type, desc_index, ctx->user_data, data_out,
                                               data_len);
            }
            break;
    }

    return USB_CONTROL_STALL;
}

/*****************************************************************************
 * GET_STRING_DESCRIPTOR Handling
 *****************************************************************************/

int usb_control_get_string(uint8_t index, struct usb_control_context* ctx, void** data_out,
                           size_t* data_len)
{
    const uint8_t* str_desc = NULL;
    size_t str_len = 0;

    if (index == 0)
    {
        /* Language ID descriptor */
        if (ctx->lang_id_desc)
        {
            str_desc = ctx->lang_id_desc;
            str_len = ctx->lang_id_desc ? ctx->lang_id_desc[0] : 0;
        }
        else
        {
            str_desc = default_string0;
            str_len = sizeof(default_string0);
        }
    }
    else if (ctx->string_descs && index <= ctx->num_strings)
    {
        str_desc = ctx->string_descs[index - 1];
        str_len = str_desc[0]; /* First byte is length */
    }
    else if (ctx->string_handler)
    {
        return ctx->string_handler(index, ctx->user_data, data_out, data_len);
    }

    if (str_desc && str_len > 0)
    {
        *data_out = osal_malloc(str_len);
        if (*data_out)
        {
            memcpy(*data_out, str_desc, str_len);
            *data_len = str_len;
            return USB_CONTROL_OK;
        }
    }

    return USB_CONTROL_STALL;
}

/*****************************************************************************
 * SET/GET_CONFIGURATION Handling
 *****************************************************************************/

int usb_control_set_config(const struct usb_setup_packet* setup, struct usb_control_context* ctx,
                           void** data_out, size_t* data_len)
{
    (void)data_out;
    (void)data_len;

    uint8_t config_value = setup->wValue & 0xFF;

    /* Check if config value is valid */
    if (config_value == 0 || config_value <= ctx->num_configs)
    {
        ctx->config_value = config_value;

        /* Callback notification */
        if (ctx->config_handler)
        {
            ctx->config_handler(config_value, ctx->user_data);
        }

        return USB_CONTROL_OK_NO_DATA;
    }

    return USB_CONTROL_STALL;
}

int usb_control_get_config(const struct usb_setup_packet* setup, struct usb_control_context* ctx,
                           void** data_out, size_t* data_len)
{
    (void)setup;

    uint8_t* p = osal_malloc(1);
    if (p)
    {
        *p = ctx->config_value;
        *data_out = p;
        *data_len = 1;
        return USB_CONTROL_OK;
    }

    return USB_CONTROL_ERROR;
}

/*****************************************************************************
 * GET_STATUS Handling
 *****************************************************************************/

int usb_control_get_status(const struct usb_setup_packet* setup, struct usb_control_context* ctx,
                           void** data_out, size_t* data_len)
{
    (void)ctx;

    uint8_t recipient = USB_SETUP_RECIPIENT(setup);
    uint16_t* status = osal_malloc(2);

    if (!status)
    {
        return USB_CONTROL_ERROR;
    }

    switch (recipient)
    {
        case USB_RECIP_DEVICE:
            /* Bus-powered, no remote wakeup support */
            *status = 0x0000; 
            break;

        case USB_RECIP_INTERFACE:
            *status = 0x0000;
            break;

        case USB_RECIP_ENDPOINT:
            /* Endpoint status (Not halted) */
            *status = 0x0000;
            break;

        default:
            osal_free(status);
            return USB_CONTROL_STALL;
    }

    *data_out = status;
    *data_len = 2;
    return USB_CONTROL_OK;
}

/*****************************************************************************
 * Class Request Handling (default implementation, can be overridden)
 *****************************************************************************/

int usb_control_class_request(const struct usb_setup_packet* setup, struct usb_control_context* ctx,
                              void** data_out, size_t* data_len)
{
    /* If class handler is provided, call it */
    if (ctx->class_handler)
    {
        return ctx->class_handler(setup, ctx->user_data, data_out, data_len);
    }

    return USB_CONTROL_STALL;
}