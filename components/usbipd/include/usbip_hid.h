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
 * Virtual HID Base - HID Generic Framework
 *
 * Provides common HID device request handling and data management
 * Can be reused by different HID device implementations
 */

#ifndef VIRTUAL_HID_H
#define VIRTUAL_HID_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * HID Constants
 *****************************************************************************/

/* HID Class Request Codes */
#define HID_REQUEST_GET_REPORT 0x01
#define HID_REQUEST_GET_IDLE 0x02
#define HID_REQUEST_GET_PROTOCOL 0x03
#define HID_REQUEST_SET_REPORT 0x09
#define HID_REQUEST_SET_IDLE 0x0A
#define HID_REQUEST_SET_PROTOCOL 0x0B

/* HID Protocol Modes */
#define HID_PROTOCOL_BOOT 0x00
#define HID_PROTOCOL_REPORT 0x01

/* HID Report Types */
#define HID_REPORT_TYPE_INPUT 0x01
#define HID_REPORT_TYPE_OUTPUT 0x02
#define HID_REPORT_TYPE_FEATURE 0x03

/* Default Report Size */
#define HID_DEFAULT_REPORT_SIZE 64

/*****************************************************************************
 * Report ID Handling Options
 *
 * HID Specification on Report ID:
 *
 * 1. Report ID is optional
 *    - If no Report ID item in report descriptor, Report ID is not used
 *    - If Report ID is used, value range must be 1-255 (0x01-0xFF)
 *    - 0x00 is reserved, means "no Report ID"
 *
 * 2. Linux hidraw driver behavior:
 *    - Only processes Report ID when defined in report descriptor
 *    - For devices without Report ID, hidraw transmits data as-is
 *    - hidraw does not add or remove 0x00 automatically
 *
 * 3. Devices in this project:
 *    - CMSIS-DAP: no Report ID in report descriptor, 64-byte report size
 *    - Handling: pass through as-is, no add/strip
 *****************************************************************************/

/* Report ID Handling Modes */
#define HID_REPORT_ID_NONE 0x00    /* No processing, pass through */
#define HID_REPORT_ID_AUTO 0x01    /* Auto detect (optimized for CMSIS-DAP) */
#define HID_REPORT_ID_STRIP 0x02   /* Always strip first byte as Report ID */
#define HID_REPORT_ID_PREPEND 0x03 /* Always prepend Report ID (compat mode) */

/*****************************************************************************
 * HID Device Callback Interface
 *****************************************************************************/

struct hid_device_ops
{
    /**
     * handle_data - Handle HID OUT data
     * @report_id: Report ID (already processed)
     * @data: Data (Report ID stripped)
     * @len: Data length
     * @user_data: User data pointer
     * Return: 0 on success, negative on failure
     */
    int (*handle_data)(uint8_t report_id, const void* data, size_t len, void* user_data);

    /**
     * get_report - Get HID report
     * @report_type: Report type (INPUT/OUTPUT/FEATURE)
     * @report_id: Report ID
     * @data: Output data buffer
     * @len: Output data length
     * @user_data: User data pointer
     * Return: 0 on success, negative on failure
     */
    int (*get_report)(uint8_t report_type, uint8_t report_id, void* data, size_t* len,
                      void* user_data);

    /**
     * get_idle - Get idle rate
     */
    int (*get_idle)(uint8_t report_id, uint8_t* duration, void* user_data);

    /**
     * set_idle - Set idle rate
     */
    int (*set_idle)(uint8_t report_id, uint8_t duration, void* user_data);

    /**
     * get_protocol - Get current protocol
     */
    int (*get_protocol)(uint8_t* protocol, void* user_data);

    /**
     * set_protocol - Set protocol
     */
    int (*set_protocol)(uint8_t protocol, void* user_data);
};

/*****************************************************************************
 * HID Device Context
 *****************************************************************************/

struct hid_device_ctx
{
    const struct hid_device_ops* ops;
    void* user_data;
    uint8_t report_size;
    uint8_t report_id_mode;
    uint8_t protocol;
    uint8_t idle_duration;
};

/*****************************************************************************
 * HID Common Function Interface
 *****************************************************************************/

/**
 * hid_init_ctx - Initialize HID device context
 * @ctx: HID device context
 * @ops: Device operation callbacks
 * @report_size: Report size
 * @user_data: User data pointer
 */
void hid_init_ctx(struct hid_device_ctx* ctx, const struct hid_device_ops* ops, uint8_t report_size,
                  void* user_data);

/**
 * hid_class_request_handler - HID class request handler
 */
int hid_class_request_handler(struct hid_device_ctx* ctx, const void* setup, void** data_out,
                              size_t* data_len);

/**
 * hid_handle_out_report - Handle HID OUT report
 * @ctx: HID device context
 * @data: Raw data (may contain Report ID)
 * @len: Data length
 *
 * Automatically process Report ID, then call handle_data callback
 */
int hid_handle_out_report(struct hid_device_ctx* ctx, const void* data, size_t len);

/**
 * hid_normalize_report_id - Normalize Report ID
 * @ctx: HID device context
 * @input: Input data
 * @input_len: Input data length
 * @output: Output data buffer
 * @output_len: Output data length pointer
 * @report_id: Output Report ID (0 = no Report ID)
 * Return: 0 on success, negative on failure
 *
 * Process Report ID according to HID specification:
 * - Report ID value range: 1-255 (0x01-0xFF)
 * - 0x00 is reserved, means "no Report ID"
 * - CMSIS-DAP devices have no Report ID, pass 64-byte data as-is
 *
 * Mode descriptions:
 * - HID_REPORT_ID_NONE: Pass through, no processing
 * - HID_REPORT_ID_AUTO: Optimized for CMSIS-DAP, pass 64-byte data as-is
 * - HID_REPORT_ID_STRIP: Strip first byte as Report ID
 * - HID_REPORT_ID_PREPEND: Prepend Report ID 0x00 (backward compat)
 */
int hid_normalize_report_id(struct hid_device_ctx* ctx, const void* input, size_t input_len,
                            void* output, size_t* output_len, uint8_t* report_id);

#ifdef __cplusplus
}
#endif

#endif /* VIRTUAL_HID_H */
