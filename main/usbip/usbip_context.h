/**
 * @file usbip_context.h
 * @brief USBIP context structure definition
 *
 * This module defines the context structure that holds all state
 * for a USBIP server instance, replacing global variables.
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "transport/transport.h"
#include "components/USBIP/usbip_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USBIP server states
 */
typedef enum {
    USBIP_STATE_ACCEPTING = 0,
    USBIP_STATE_ATTACHING,
    USBIP_STATE_EMULATING,
    USBIP_STATE_EL_DATA_PHASE
} usbip_state_t;

/**
 * @brief USBIP context structure
 *
 * Holds all state for a USBIP server instance.
 */
typedef struct {
    transport_context_t *transport;     /**< Transport layer context */
    usbip_state_t state;                /**< Current USBIP state */
    uint8_t *rx_buffer;                 /**< Receive buffer */
    size_t rx_buffer_size;              /**< Receive buffer size */
} usbip_context_t;

/**
 * @brief Create USBIP context
 *
 * @param ops Transport operations
 * @param port Port number to listen on
 * @return USBIP context pointer, or NULL on failure
 */
usbip_context_t *usbip_context_create(const transport_ops_t *ops, uint16_t port);

/**
 * @brief Destroy USBIP context
 *
 * @param ctx USBIP context
 */
void usbip_context_destroy(usbip_context_t *ctx);

/**
 * @brief Get current USBIP state
 *
 * @param ctx USBIP context
 * @return Current state
 */
usbip_state_t usbip_context_get_state(usbip_context_t *ctx);

/**
 * @brief Set USBIP state
 *
 * @param ctx USBIP context
 * @param state New state
 */
void usbip_context_set_state(usbip_context_t *ctx, usbip_state_t state);

/**
 * @brief Initialize USBIP server (create listening socket)
 *
 * @param ctx USBIP context
 * @return 0 on success, negative on error
 */
int usbip_context_init(usbip_context_t *ctx);

/**
 * @brief Accept incoming connection
 *
 * @param ctx USBIP context
 * @return 0 on success, negative on error
 */
int usbip_context_accept(usbip_context_t *ctx);

/**
 * @brief Send data through transport
 *
 * @param ctx USBIP context
 * @param buf Data buffer
 * @param len Data length
 * @return Bytes sent, or negative on error
 */
int usbip_context_send(usbip_context_t *ctx, const void *buf, size_t len);

/**
 * @brief Receive data from transport
 *
 * @param ctx USBIP context
 * @param buf Buffer to receive data
 * @param len Maximum length
 * @return Bytes received, 0 on close, negative on error
 */
int usbip_context_recv(usbip_context_t *ctx, void *buf, size_t len);

/**
 * @brief Close current connection
 *
 * @param ctx USBIP context
 * @return 0 on success, negative on error
 */
int usbip_context_close(usbip_context_t *ctx);

/**
 * @brief Reset USBIP context state
 *
 * @param ctx USBIP context
 */
void usbip_context_reset(usbip_context_t *ctx);

#ifdef __cplusplus
}
#endif