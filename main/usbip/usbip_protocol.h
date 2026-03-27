/**
 * @file usbip_protocol.h
 * @brief USBIP protocol utility functions
 *
 * This module provides utility functions for building and sending
 * USBIP protocol responses.
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "usbip/usbip_context.h"
#include "usbip_defs.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Send stage2 submit response
 *
 * @param ctx USBIP context
 * @param req_header Request header (will be modified)
 * @param status Status code
 * @param data_length Data length for the response
 */
void usbip_send_stage2_submit(usbip_context_t *ctx, usbip_stage2_header *req_header,
                               int32_t status, int32_t data_length);

/**
 * @brief Send stage2 submit response with data
 *
 * @param ctx USBIP context
 * @param req_header Request header (will be modified)
 * @param status Status code
 * @param data Data to send
 * @param data_length Data length
 */
void usbip_send_stage2_submit_data(usbip_context_t *ctx, usbip_stage2_header *req_header,
                                    int32_t status, const void *data, int32_t data_length);

/**
 * @brief Send stage2 submit response with data - fast path
 *
 * Combines header and data into a single send call for better performance.
 *
 * @param ctx USBIP context
 * @param req_header Request header (will be modified, must have space for data after it)
 * @param data Data to send
 * @param data_length Data length
 */
void usbip_send_stage2_submit_data_fast(usbip_context_t *ctx, usbip_stage2_header *req_header,
                                         const void *data, int32_t data_length);

/**
 * @brief Fast reply for EP1 IN requests (DAP response)
 *
 * Handles DAP data IN requests by returning the response
 * that was already processed synchronously.
 *
 * @param ctx USBIP context
 * @param buf Data buffer containing the request header
 * @param length Data length
 * @return 1 if handled, 0 if not a fast reply request
 */
int fast_reply(usbip_context_t *ctx, uint8_t *buf, uint32_t length);

/**
 * @brief Swap header byte order (host <-> network)
 *
 * Converts header fields between host and network byte order.
 * This operation is symmetric (same for pack and unpack).
 *
 * @param data Pointer to header data
 * @param size Header size in bytes
 */
void usbip_swap_header(void *data, size_t size);

#ifdef __cplusplus
}
#endif