/**
 * @file transport.h
 * @brief Transport interface abstraction for USBIP protocol
 *
 * This module provides a polymorphic transport layer that abstracts
 * different network implementations (TCP, KCP, Netconn) behind a
 * unified interface.
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Transport connection states
 */
typedef enum
{
    TRANSPORT_STATE_DISCONNECTED = 0,
    TRANSPORT_STATE_LISTENING,
    TRANSPORT_STATE_CONNECTED
} transport_state_t;

/**
 * @brief Forward declarations
 */
typedef struct transport_context transport_context_t;
typedef struct transport_ops transport_ops_t;

/**
 * @brief Transport operations interface (virtual function table)
 *
 * Each transport implementation (TCP, KCP, Netconn) must provide
 * these operations to enable polymorphic behavior.
 */
struct transport_ops
{
    /**
     * @brief Initialize transport (create listening socket, etc.)
     * @param ctx Transport context
     * @return 0 on success, negative on error
     */
    int (*init)(transport_context_t *ctx);

    /**
     * @brief Accept incoming connection (blocking)
     * @param ctx Transport context
     * @return 0 on success, negative on error
     */
    int (*accept)(transport_context_t *ctx);

    /**
     * @brief Send data through transport
     * @param ctx Transport context
     * @param buf Data buffer to send
     * @param len Length of data
     * @return Number of bytes sent, negative on error
     */
    int (*send)(transport_context_t *ctx, const void *buf, size_t len);

    /**
     * @brief Receive data from transport (blocking)
     * @param ctx Transport context
     * @param buf Buffer to receive data
     * @param len Maximum length to receive
     * @return Number of bytes received, 0 on connection close, negative on error
     */
    int (*recv)(transport_context_t *ctx, void *buf, size_t len);

    /**
     * @brief Close current connection
     * @param ctx Transport context
     * @return 0 on success, negative on error
     */
    int (*close)(transport_context_t *ctx);

    /**
     * @brief Destroy transport and free all resources
     * @param ctx Transport context
     */
    void (*destroy)(transport_context_t *ctx);
};

/**
 * @brief Transport context structure
 *
 * Holds the state and private data for a transport instance.
 */
struct transport_context
{
    const transport_ops_t *ops;     /**< Operation function table */
    void *private_data;             /**< Implementation-specific data */
    uint8_t *rx_buffer;             /**< Shared receive buffer */
    size_t rx_buffer_size;          /**< Receive buffer size */
    transport_state_t state;        /**< Current connection state */
    uint16_t port;                  /**< Listening port */
};

/**
 * @brief Create transport context
 *
 * @param ops Transport operations (must not be NULL)
 * @param rx_buffer_size Size of receive buffer to allocate
 * @param port Port number to listen on
 * @return Transport context pointer, or NULL on failure
 */
transport_context_t *transport_create(const transport_ops_t *ops,
                                       size_t rx_buffer_size,
                                       uint16_t port);

/**
 * @brief Initialize transport (create listening socket)
 *
 * @param ctx Transport context
 * @return 0 on success, negative on error
 */
int transport_init(transport_context_t *ctx);

/**
 * @brief Accept incoming connection (blocking)
 *
 * @param ctx Transport context
 * @return 0 on success, negative on error
 */
int transport_accept(transport_context_t *ctx);

/**
 * @brief Send data through transport
 *
 * @param ctx Transport context
 * @param buf Data buffer to send
 * @param len Length of data
 * @return Number of bytes sent, negative on error
 */
int transport_send(transport_context_t *ctx, const void *buf, size_t len);

/**
 * @brief Receive data from transport (blocking)
 *
 * @param ctx Transport context
 * @param buf Buffer to receive data (can be NULL to use internal buffer)
 * @param len Maximum length to receive
 * @return Number of bytes received, 0 on connection close, negative on error
 */
int transport_recv(transport_context_t *ctx, void *buf, size_t len);

/**
 * @brief Get pointer to internal receive buffer
 *
 * @param ctx Transport context
 * @return Pointer to receive buffer
 */
uint8_t *transport_get_rx_buffer(transport_context_t *ctx);

/**
 * @brief Close current connection
 *
 * @param ctx Transport context
 * @return 0 on success, negative on error
 */
int transport_close(transport_context_t *ctx);

/**
 * @brief Destroy transport and free all resources
 *
 * @param ctx Transport context
 */
void transport_destroy(transport_context_t *ctx);

/**
 * @brief Get current transport state
 *
 * @param ctx Transport context
 * @return Current state
 */
transport_state_t transport_get_state(transport_context_t *ctx);

#ifdef __cplusplus
}
#endif