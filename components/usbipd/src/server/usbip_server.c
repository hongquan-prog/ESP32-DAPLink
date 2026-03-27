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
 * USBIP Server Core
 *
 * Server core logic: connection management, device enumeration
 */
#include <endian.h>
#include <stdio.h>
#include <string.h>

#include "hal/usbip_log.h"
#include "hal/usbip_osal.h"
#include "hal/usbip_transport.h"
#include "usbip_protocol.h"
#include "usbip_server.h"

LOG_MODULE_REGISTER(server, CONFIG_USBIP_LOG_LEVEL);

/*****************************************************************************
 * Global State
 *****************************************************************************/

static volatile int s_running = 1;

/*****************************************************************************
 * Static Function Declarations
 *****************************************************************************/

static int usbip_server_handle_devlist(struct usbip_conn_ctx* ctx);
static int usbip_server_handle_import_req(struct usbip_conn_ctx* ctx);

/*
 * OP_REQ_DEVLIST Processing
 */
static int usbip_server_handle_devlist(struct usbip_conn_ctx* ctx)
{
    struct usbip_usb_device udev;
    struct usbip_usb_interface iface;
    struct usbip_device_driver* driver;
    uint32_t reply_count;
    int device_count = 0;
    int drv_idx;

    /* Send operation header */
    if (usbip_send_op_common(ctx, OP_REP_DEVLIST, ST_OK) < 0)
    {
        LOG_ERR("Failed to send OP_REP_DEVLIST header");

        return -1;
    }

    /* Count total devices from all drivers */
    for (driver = usbip_get_first_driver(); driver != NULL; driver = usbip_get_next_driver(driver))
    {
        device_count += driver->get_device_count(driver);
    }

    /* Send device count */
    reply_count = htobe32(device_count);

    if (transport_send(ctx, &reply_count, sizeof(reply_count)) != sizeof(reply_count))
    {
        LOG_ERR("Failed to send device count");

        return -1;
    }

    LOG_DBG("Sending %d device(s)", device_count);

    /* Send each device from all drivers */
    for (driver = usbip_get_first_driver(); driver != NULL; driver = usbip_get_next_driver(driver))
    {
        int drv_count = driver->get_device_count(driver);

        for (drv_idx = 0; drv_idx < drv_count; drv_idx++)
        {
            if (driver->get_device_by_index(driver, drv_idx, &udev) < 0)
            {
                continue;
            }

            usbip_pack_usb_device(&udev, 1);

            if (transport_send(ctx, &udev, sizeof(udev)) != sizeof(udev))
            {
                LOG_ERR("Failed to send device");

                return -1;
            }

            /* Send interface information */
            memset(&iface, 0, sizeof(iface));
            iface.bInterfaceClass = 0x03;
            iface.bInterfaceSubClass = 0x01;
            iface.bInterfaceProtocol = 0x01;
            usbip_pack_usb_interface(&iface, 1);

            if (transport_send(ctx, &iface, sizeof(iface)) != sizeof(iface))
            {
                LOG_ERR("Failed to send interface");

                return -1;
            }
        }
    }

    return 0;
}

/*****************************************************************************
 * OP_REQ_IMPORT Processing
 *****************************************************************************/

static int usbip_server_handle_import_req(struct usbip_conn_ctx* ctx)
{
    char busid[SYSFS_BUS_ID_SIZE];
    struct usbip_device_driver* driver = NULL;
    const struct usbip_usb_device* found_dev = NULL;
    int found = 0;

    /* Receive busid */
    memset(busid, 0, sizeof(busid));
    if (transport_recv(ctx, busid, sizeof(busid)) != sizeof(busid))
    {
        LOG_ERR("Failed to receive busid");
        usbip_send_op_common(ctx, OP_REP_IMPORT, ST_ERROR);
        return -1;
    }

    /* Find device */
    for (driver = usbip_get_first_driver(); driver != NULL && !found;
         driver = usbip_get_next_driver(driver))
    {

        found_dev = driver->get_device(driver, busid);
        if (found_dev)
        {
            found = 1;
            break;
        }
    }

    if (!found)
    {
        LOG_WRN("Device not found: %s", busid);
        usbip_send_op_common(ctx, OP_REP_IMPORT, ST_NODEV);
        return -1;
    }

    if (usbip_is_device_busy(busid))
    {
        LOG_WRN("Device busy: %s", busid);
        usbip_send_op_common(ctx, OP_REP_IMPORT, ST_DEV_BUSY);
        return -1;
    }

    /* Send success response */
    if (usbip_send_op_common(ctx, OP_REP_IMPORT, ST_OK) < 0)
    {
        LOG_ERR("Failed to send OP_REP_IMPORT header");
        return -1;
    }

    /* Send device descriptor */
    struct usbip_usb_device reply_dev;
    memcpy(&reply_dev, found_dev, sizeof(reply_dev));
    usbip_pack_usb_device(&reply_dev, 1);

    if (transport_send(ctx, &reply_dev, sizeof(reply_dev)) != sizeof(reply_dev))
    {
        LOG_ERR("Failed to send device description");
        return -1;
    }

    /* Export device */
    if (driver->export_device(driver, busid, ctx) < 0)
    {
        LOG_ERR("Failed to export device: %s", busid);
        return -1;
    }

    LOG_DBG("Device imported: %s", busid);

    /* Enter URB processing loop */
    return usbip_urb_loop(ctx, driver, busid);
}

/*****************************************************************************
 * Connection Handling
 *****************************************************************************/

void usbip_server_handle_connection(struct usbip_conn_ctx* ctx)
{
    struct op_common op;

    LOG_DBG("Handling new connection");

    /* Receive operation header */
    if (usbip_recv_op_common(ctx, &op) < 0)
    {
        LOG_DBG("Failed to receive op_common");
        transport_close(ctx);
        return;
    }

    LOG_DBG("Received op: version=0x%04x code=0x%04x status=0x%08x", op.version, op.code,
            op.status);

    /* Check version */
    if (op.version != USBIP_VERSION)
    {
        LOG_ERR("Unsupported version: 0x%04x", op.version);
        usbip_send_op_common(ctx, op.code | OP_REPLY, ST_ERROR);
        transport_close(ctx);
        return;
    }

    /* Handle operation */
    switch (op.code)
    {
        case OP_REQ_DEVLIST:
            usbip_server_handle_devlist(ctx);
            break;

        case OP_REQ_IMPORT:
            usbip_server_handle_import_req(ctx);
            break;

        default:
            LOG_WRN("Unknown operation: 0x%04x", op.code);
            usbip_send_op_common(ctx, op.code | OP_REPLY, ST_NA);
            break;
    }

    transport_close(ctx);
}

/*****************************************************************************
 * Server interface
 *****************************************************************************/

int usbip_server_init(uint16_t port)
{
    if (transport_listen(port) < 0)
    {
        LOG_ERR("Failed to listen on port %d", port);
        return -1;
    }

    LOG_INF("Server listening on port %d", port);

    return 0;
}

int usbip_server_run(void)
{
    struct usbip_conn_ctx* ctx;

    while (s_running)
    {
        ctx = transport_accept();
        if (ctx)
        {
            usbip_server_handle_connection(ctx);
        }
    }

    return 0;
}

void usbip_server_stop(void)
{
    s_running = 0;
}

void usbip_server_cleanup(void)
{
    transport_destroy();
}