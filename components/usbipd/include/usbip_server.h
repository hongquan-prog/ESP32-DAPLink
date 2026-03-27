/*
 * Copyright (c) 2026-2026, hongquan.li
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-3-24      hongquan.li   add license declaration
 */

#ifndef USBIP_SERVER_H
#define USBIP_SERVER_H

#include "hal/usbip_transport.h"
#include "usbip_devmgr.h"
#include "usbip_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Server Configuration
 *****************************************************************************/

#define USBIP_DEFAULT_PORT 3240
#define USBIP_MAX_CLIENTS 16
#define USBIP_MAX_DRIVERS 8
#define USBIP_URB_QUEUE_SIZE 8      /* URB queue slots */
#define USBIP_URB_DATA_MAX_SIZE 256 /* Max data size per URB */

/*****************************************************************************
 * Server Main Interface
 *****************************************************************************/

/**
 * usbip_server_init - Initialize server
 * @port: Listen port
 * Return: 0 on success, negative on failure
 */
int usbip_server_init(uint16_t port);

/**
 * usbip_server_run - Run server main loop
 * Return: 0 normal exit, negative on failure
 */
int usbip_server_run(void);

/**
 * usbip_server_stop - Stop server
 */
void usbip_server_stop(void);

/**
 * usbip_server_cleanup - Cleanup server resources
 */
void usbip_server_cleanup(void);

/*****************************************************************************
 * Connection Handling Interface
 *****************************************************************************/

/**
 * handle_connection - Handle single connection
 * @transport: Transport instance
 * @conn_fd: Connection handle
 *
 * Handle client requests until connection closes
 */
void handle_connection(struct usbip_transport* transport, int conn_fd);

/**
 * server_urb_loop - URB processing loop
 * @ctx: Connection context
 * @driver: Device driver
 * @busid: Device bus ID
 * Return: 0 normal exit, negative on failure
 */
int usbip_urb_loop(struct usbip_conn_ctx* ctx, struct usbip_device_driver* driver,
                   const char* busid);

#ifdef __cplusplus
}
#endif

#endif /* USBIP_SERVER_H */