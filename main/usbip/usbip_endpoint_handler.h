/**
 * @file usbip_endpoint_handler.h
 * @brief USBIP endpoint handler abstraction layer
 *
 * This module provides an abstraction layer for handling USBIP
 * endpoint requests, decoupling protocol handling from business logic.
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "usbip/usbip_context.h"
#include "components/USBIP/usbip_defs.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Endpoint handler interface
 *
 * Each endpoint can register its own handler for processing requests.
 */
typedef struct usbip_endpoint_handler
{
    /**
     * @brief Handle data OUT request (host -> device)
     *
     * @param ctx USBIP context
     * @param header Request header
     * @param length Total packet length
     */
    void (*handle_out)(usbip_context_t *ctx, usbip_stage2_header *header, uint32_t length);

    /**
     * @brief Handle data IN request (device -> host)
     *
     * @param ctx USBIP context
     * @param header Request header
     */
    void (*handle_in)(usbip_context_t *ctx, usbip_stage2_header *header);

    /**
     * @brief Handle unlink request
     */
    void (*handle_unlink)(void);

} usbip_endpoint_handler_t;

/**
 * @brief Register endpoint handler
 *
 * @param endpoint Endpoint number (0-127)
 * @param handler Handler structure (must remain valid)
 */
void usbip_endpoint_handler_register(uint8_t endpoint, const usbip_endpoint_handler_t *handler);

/**
 * @brief Get endpoint handler
 *
 * @param endpoint Endpoint number
 * @return Handler or NULL if not registered
 */
const usbip_endpoint_handler_t *usbip_endpoint_handler_get(uint8_t endpoint);

#ifdef __cplusplus
}
#endif