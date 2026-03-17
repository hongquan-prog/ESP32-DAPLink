/**
 * @file usb_handle.h
 * @brief Handle all Standard Device Requests on endpoint 0
 *
 * @copyright Copyright (c) 2020
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "components/USBIP/usbip_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USB send operations callback structure
 *
 * Provides send capabilities from upper layer, allowing this
 * component to remain independent of the application layer.
 */
typedef struct
{
    void *user_data;    /**< User data passed to callbacks (e.g., usbip_context_t) */

    /**
     * @brief Send stage2 submit response with data
     * @param user_data User data
     * @param req_header Request header
     * @param status Status code
     * @param data Data to send
     * @param data_length Data length
     */
    void (*send_submit_data)(void *user_data, usbip_stage2_header *req_header,
                              int32_t status, const void *data, int32_t data_length);

    /**
     * @brief Send stage2 submit response (no data)
     * @param user_data User data
     * @param req_header Request header
     * @param status Status code
     * @param data_length Data length
     */
    void (*send_submit)(void *user_data, usbip_stage2_header *req_header,
                         int32_t status, int32_t data_length);

    /**
     * @brief Send raw data through transport
     * @param user_data User data
     * @param data Data to send
     * @param len Data length
     */
    void (*send_data)(void *user_data, const void *data, size_t len);
} usb_send_ops_t;

/**
 * @brief Handle USB control request on endpoint 0
 *
 * Processes standard device requests including GET_DESCRIPTOR,
 * SET_CONFIGURATION, and vendor-specific requests.
 *
 * @param header USBIP stage2 header containing the request
 * @param ops Send operations callback structure (must not be NULL)
 */
void handleUSBControlRequest(usbip_stage2_header *header, const usb_send_ops_t *ops);

#ifdef __cplusplus
}
#endif