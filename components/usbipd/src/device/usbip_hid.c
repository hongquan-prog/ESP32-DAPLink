/*
 * Copyright (c) 2026-2026, hongquan.li
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-3-24      hongquan.li   add license declaration
 */

/*
 * Virtual HID Base - HID Generic Framework Implementation
 *
 * Provides common HID device request handling
 */

#include "usbip_hid.h"
#include <string.h>
#include "hal/usbip_log.h"
#include "hal/usbip_osal.h"
#include "usbip_common.h"
#include "usbip_hid.h"

LOG_MODULE_REGISTER(hid, CONFIG_USBIP_LOG_LEVEL);

/**************************************************************************
 * HID Context Initialization
 **************************************************************************/

void hid_init_ctx(struct hid_device_ctx* ctx, const struct hid_device_ops* ops, uint8_t report_size,
                  void* user_data)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->ops = ops;
    ctx->report_size = report_size;
    ctx->user_data = user_data;
    ctx->report_id_mode = HID_REPORT_ID_AUTO;
    ctx->protocol = HID_PROTOCOL_REPORT;
}

/**************************************************************************
 * Report ID Normalization Processing
 *
 * HID Specification on Report ID:
 *
 * 1. Report ID is optional
 *    - If no Report ID item in report descriptor, Report ID is not used
 *    - If Report ID is used, value range must be 1-255 (0x01-0xFF)
 *    - 0x00 is reserved, means "no Report ID"
 *
 * 2. Data packet format:
 *    - Without Report ID: [Data 0][Data 1]...[Data n-1] (n = report size)
 *    - With Report ID: [Report ID][Data 0]...[Data n-2] (n = report size)
 *
 * 3. Linux hidraw driver behavior:
 *    - Only processes Report ID when defined in report descriptor
 *    - For devices without Report ID, hidraw transmits data as-is
 *    - hidraw does not add or remove 0x00 automatically
 *
 * Devices in this project:
 *    - CMSIS-DAP: no Report ID in report descriptor, 64-byte report size
 *    - Handling: pass through as-is, no add/strip
 **************************************************************************/
int hid_normalize_report_id(struct hid_device_ctx* ctx, const void* input, size_t input_len,
                            void* output, size_t* output_len, uint8_t* report_id)
{
    const uint8_t* src;
    uint8_t* dst;
    size_t report_size;

    if (!input || !output || !output_len)
    {
        return -1;
    }

    src = (const uint8_t*)input;
    dst = (uint8_t*)output;
    report_size = ctx->report_size ? ctx->report_size : HID_DEFAULT_REPORT_SIZE;

    /*
     * report_id = 0 means no Report ID
     * According to HID spec, 0x00 is reserved and should not be used as actual Report ID
     */
    *report_id = 0;

    switch (ctx->report_id_mode)
    {
        case HID_REPORT_ID_NONE:
            /*
             * No processing, copy as-is
             * For: cases where no Report ID handling is needed
             */
            memcpy(dst, src, input_len);
            *output_len = input_len;
            break;

        case HID_REPORT_ID_STRIP:
            /*
             * Always strip first byte as Report ID
             * For: devices where report descriptor defines Report ID
             *
             * Note:
             * - Report ID should be 1-255
             * - 0x00 is also treated as Report ID in this mode (non-compliant)
             */
            if (input_len > 1)
            {
                *report_id = src[0];
                memcpy(dst, src + 1, input_len - 1);
                *output_len = input_len - 1;
            }
            else if (input_len == 1)
            {
                *report_id = src[0];
                *output_len = 0;
            }
            else
            {
                *output_len = 0;
            }
            break;

        case HID_REPORT_ID_PREPEND:
            /*
             * Always prepend Report ID
             *
             * Note:
             * - This mode is reserved for backward compatibility
             * - According to HID spec, 0x00 should not be used as Report ID
             * - If Report ID is needed, use value in 1-255 range
             */
            if (report_size < 2)
            {
                /* Report size too small to add Report ID */
                memcpy(dst, src, input_len < report_size ? input_len : report_size);
                *output_len = input_len < report_size ? input_len : report_size;
            }
            else
            {
                /*
                 * Use Report ID 0x00 (non-compliant but reserved for compatibility)
                 * New code should consider using Report ID in 1-255 range
                 */
                dst[0] = 0x00;
                size_t copy_len = (input_len < report_size - 1) ? input_len : (report_size - 1);
                memcpy(dst + 1, src, copy_len);
                *output_len = copy_len + 1;
                *report_id = 0x00;
            }
            break;

        case HID_REPORT_ID_AUTO:
        default:
            /*
             * Auto-detect mode - optimized for CMSIS-DAP devices in this project
             *
             * CMSIS-DAP device characteristics:
             * - No Report ID definition in report descriptor
             * - Report size = 64 bytes
             * - Data format: [CMD][DATA...], CMD range 0x00-0x7F
             *
             * Processing strategy:
             * 1. If input_len == report_size (64):
             *    - This is normal case, pass through as-is
             *    - First byte is CMSIS-DAP command, not Report ID
             *
             * 2. If input_len == report_size - 1 (63):
             *    - Some drivers may have stripped data
             *    - Pass through as-is, no padding 0x00 (because 0x00 is not valid Report ID)
             *
             * 3. Other cases: pass through as-is
             *
             * Note: No longer use heuristic (src[0] > 0x1F) because:
             * - This violates HID specification
             * - CMSIS-DAP commands can be any value in 0x00-0x7F range
             */
            if (input_len == report_size)
            {
                /*
                 * Normal case: full 64-byte report
                 * CMSIS-DAP has no Report ID, pass through as-is
                 */
                memcpy(dst, src, input_len);
                *output_len = input_len;
                *report_id = 0;
            }
            else if (input_len == report_size - 1)
            {
                /* 
                 * Some Windows HID drivers may send 63 bytes
                 * Pass through as-is, pad with 0 at end to ensure DAP doesn't read garbage
                 */
                memcpy(dst, src, input_len);
                dst[input_len] = 0x00;
                *output_len = report_size;
                *report_id = 0;
                LOG_DBG("IN: Padded %zu bytes to %zu", input_len, report_size);
            }
            else
            {
                /* Other cases, copy as-is */
                memcpy(dst, src, input_len);
                *output_len = input_len;
                *report_id = 0;
            }
            break;
    }

    return 0;
}

/**************************************************************************
 * HID OUT Report Handling
 **************************************************************************/

int hid_handle_out_report(struct hid_device_ctx* ctx, const void* data, size_t len)
{
    uint8_t buf[HID_DEFAULT_REPORT_SIZE];
    size_t out_len;
    uint8_t report_id;
    int ret;

    if (!ctx->ops || !ctx->ops->handle_data)
    {
        return -1;
    }

    /* Normalize Report ID */
    ret = hid_normalize_report_id(ctx, data, len, buf, &out_len, &report_id);
    if (ret < 0)
    {
        return ret;
    }

    /* Call device-specific handler */
    return ctx->ops->handle_data(report_id, buf, out_len, ctx->user_data);
}

/**************************************************************************
 * HID Class Request Handling
 **************************************************************************/

int hid_class_request_handler(struct hid_device_ctx* ctx, const void* setup, void** data_out,
                              size_t* data_len)
{
    const struct usb_setup_packet* req;
    const struct hid_device_ops* ops;
    uint8_t report_type;
    uint8_t report_id;
    uint8_t duration;
    uint8_t protocol;
    void* buf;
    size_t report_size;
    int ret;

    req = (const struct usb_setup_packet*)setup;
    ops = ctx->ops;
    report_size = ctx->report_size ? ctx->report_size : HID_DEFAULT_REPORT_SIZE;

    switch (req->bRequest)
    {
        case HID_REQUEST_GET_REPORT:
            report_type = (req->wValue >> 8) & 0xFF;
            report_id = req->wValue & 0xFF;

            buf = osal_malloc(report_size);
            if (!buf)
            {
                return -1;
            }

            *data_len = report_size;
            if (ops && ops->get_report)
            {
                ret = ops->get_report(report_type, report_id, buf, data_len, ctx->user_data);
                if (ret < 0)
                {
                    osal_free(buf);
                    return -1;
                }
            }
            else
            {
                memset(buf, 0, report_size);
            }

            *data_out = buf;
            return 0;

        case HID_REQUEST_GET_IDLE:
            report_id = req->wValue & 0xFF;

            buf = osal_malloc(1);
            if (!buf)
            {
                return -1;
            }

            if (ops && ops->get_idle)
            {
                ret = ops->get_idle(report_id, &duration, ctx->user_data);
                *(uint8_t*)buf = (ret == 0) ? duration : ctx->idle_duration;
            }
            else
            {
                *(uint8_t*)buf = ctx->idle_duration;
            }

            *data_out = buf;
            *data_len = 1;
            return 0;

        case HID_REQUEST_GET_PROTOCOL:
            buf = osal_malloc(1);
            if (!buf)
            {
                return -1;
            }

            if (ops && ops->get_protocol)
            {
                ret = ops->get_protocol(&protocol, ctx->user_data);
                *(uint8_t*)buf = (ret == 0) ? protocol : ctx->protocol;
            }
            else
            {
                *(uint8_t*)buf = ctx->protocol;
            }

            *data_out = buf;
            *data_len = 1;
            return 0;

        case HID_REQUEST_SET_REPORT:
            /* Data transferred in OUT stage */
            return 0;

        case HID_REQUEST_SET_IDLE:
            duration = (req->wValue >> 8) & 0xFF;
            report_id = req->wValue & 0xFF;

            ctx->idle_duration = duration;

            if (ops && ops->set_idle)
            {
                ops->set_idle(report_id, duration, ctx->user_data);
            }

            *data_len = 0;
            return 0;

        case HID_REQUEST_SET_PROTOCOL:
            protocol = req->wValue & 0xFF;

            if (protocol <= HID_PROTOCOL_REPORT)
            {
                ctx->protocol = protocol;
                if (ops && ops->set_protocol)
                {
                    ops->set_protocol(protocol, ctx->user_data);
                }
            }

            *data_len = 0;
            return 0;
    }

    return -1;
}
