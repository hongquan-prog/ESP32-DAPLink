/*
 * Copyright (c) 2026-2026, hongquan.li
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-3-24      hongquan.li   add license declaration
 */

#ifndef USBIP_TRANSPORT_H
#define USBIP_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Transport Layer Abstraction Interface
 *
 * Purpose: Implement this interface to support different transport methods
 *          (TCP/Serial/Custom). Uses connection context (ctx) instead of
 *          file descriptors for better portability.
 *****************************************************************************/

/**
 * usbip_conn_ctx - Connection context
 *
 * Application layer holds this pointer, does not directly operate on
 * underlying handles.
 */
struct usbip_conn_ctx
{
    void* priv; /* Transport-specific private data */
};

/**
 * transport_ops - Transport operations interface
 */
typedef struct transport_ops transport_ops_t;

struct transport_ops
{
    /**
     * create - Create transport instance
     * Return: Transport instance pointer, NULL on failure
     */
    struct usbip_transport* (*create)(void);

    /**
     * name - Transport name
     */
    const char* name;
};

/**
 * usbip_transport - Transport instance
 */
struct usbip_transport
{
    void* priv; /* Transport-specific private data */

    /**
     * listen - Start listening
     * @trans: Transport instance
     * @port: Port number (TCP) or other identifier
     * Return: 0 on success, negative on failure
     */
    int (*listen)(struct usbip_transport* trans, uint16_t port);

    /**
     * accept - Accept new connection
     * @trans: Transport instance
     * Return: Connection context pointer, NULL on failure
     */
    struct usbip_conn_ctx* (*accept)(struct usbip_transport* trans);

    /**
     * recv - Receive data
     * @ctx: Connection context
     * @buf: Receive buffer
     * @len: Expected receive length
     * Return: Actual bytes received, negative on failure, 0 means connection closed
     */
    ssize_t (*recv)(struct usbip_conn_ctx* ctx, void* buf, size_t len);

    /**
     * send - Send data
     * @ctx: Connection context
     * @buf: Send buffer
     * @len: Send length
     * Return: Actual bytes sent, negative on failure
     */
    ssize_t (*send)(struct usbip_conn_ctx* ctx, const void* buf, size_t len);

    /**
     * close - Close connection
     * @ctx: Connection context
     */
    void (*close)(struct usbip_conn_ctx* ctx);

    /**
     * destroy - Destroy transport instance
     * @trans: Transport instance
     */
    void (*destroy)(struct usbip_transport* trans);
};

/*****************************************************************************
 * Global Transport Management (Internal)
 *****************************************************************************/

/**
 * transport_register - Register transport instance (internal)
 * @name: Transport name
 * @trans: Transport instance
 */
void transport_register(const char* name, struct usbip_transport* trans);

/**
 * TRANSPORT_REGISTER - Auto-registration macro
 * @platform: Platform name
 * @ops: transport_ops_t variable name
 */
#define TRANSPORT_REGISTER(platform, ops)                                                          \
    __attribute__((constructor, used)) void transport_register_##platform(void)                   \
    {                                                                                              \
        transport_register(#platform, &ops);                                                       \
    }

/*****************************************************************************
 * Default Transport Management (Wrapper Functions)
 *
 * These functions operate on a global default transport instance
 *****************************************************************************/

/**
 * transport_listen - Start listening on default transport
 * @port: Port number
 * Return: 0 on success, negative on failure
 */
int transport_listen(uint16_t port);

/**
 * transport_accept - Accept connection on default transport
 * Return: Connection context pointer, NULL on failure
 */
struct usbip_conn_ctx* transport_accept(void);

/**
 * transport_recv - Receive data on default transport
 * @ctx: Connection context
 * @buf: Receive buffer
 * @len: Buffer length
 * Return: Bytes received, negative on failure
 */
ssize_t transport_recv(struct usbip_conn_ctx* ctx, void* buf, size_t len);

/**
 * transport_send - Send data on default transport
 * @ctx: Connection context
 * @buf: Send buffer
 * @len: Data length
 * Return: Bytes sent, negative on failure
 */
ssize_t transport_send(struct usbip_conn_ctx* ctx, const void* buf, size_t len);

/**
 * transport_close - Close connection on default transport
 * @ctx: Connection context
 */
void transport_close(struct usbip_conn_ctx* ctx);

/**
 * transport_destroy - Destroy default transport
 */
void transport_destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* USBIP_TRANSPORT_H */