/**
 * @file usbip_service.h
 * @brief USBIP service entry point
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "usbip/usbip_context.h"
#include "transport/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USBIP service task
 *
 * Main task that handles USBIP protocol over the configured transport.
 *
 * @param arg Transport operations pointer (transport_ops_t*)
 */
void usbip_service_task(void *arg);

/**
 * @brief Process received USBIP data
 *
 * @param ctx USBIP context
 * @param buffer Data buffer
 * @param length Data length
 * @return 0 on success, negative on error
 */
int usbip_service_process_data(usbip_context_t *ctx, uint8_t *buffer, uint32_t length);

#ifdef __cplusplus
}
#endif