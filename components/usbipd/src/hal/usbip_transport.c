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
 * Transport Core
 *
 * Transport layer core - single global transport instance
 */

#include "hal/usbip_transport.h"
#include <string.h>
#include "hal/usbip_log.h"

LOG_MODULE_REGISTER(transport, CONFIG_USBIP_LOG_LEVEL);

/*****************************************************************************
 * Global Transport Instance
 *****************************************************************************/

static struct usbip_transport* s_transport = NULL;
static char s_platform_name[32] = {0};

void transport_register(const char* name, struct usbip_transport* trans)
{
    if (!name || !trans)
    {
        LOG_ERR("transport_register: name or trans is NULL");
        return;
    }

    if (s_transport != NULL)
    {
        LOG_ERR("transport_register: transport already registered");
        return;
    }

    s_transport = trans;
    strncpy(s_platform_name, name, sizeof(s_platform_name) - 1);
    s_platform_name[sizeof(s_platform_name) - 1] = '\0';
    LOG_INF("transport_register: platform %s", s_platform_name);
}

/*****************************************************************************
 * Transport Operations Wrapper Functions
 *****************************************************************************/

int transport_listen(uint16_t port)
{
    if (!s_transport || !s_transport->listen)
    {
        return -1;
    }

    return s_transport->listen(s_transport, port);
}

struct usbip_conn_ctx* transport_accept(void)
{
    if (!s_transport || !s_transport->accept)
    {
        return NULL;
    }

    return s_transport->accept(s_transport);
}

ssize_t transport_recv(struct usbip_conn_ctx* ctx, void* buf, size_t len)
{
    if (!s_transport || !s_transport->recv)
    {
        return -1;
    }

    return s_transport->recv(ctx, buf, len);
}

ssize_t transport_send(struct usbip_conn_ctx* ctx, const void* buf, size_t len)
{
    if (!s_transport || !s_transport->send)
    {
        return -1;
    }

    return s_transport->send(ctx, buf, len);
}

void transport_close(struct usbip_conn_ctx* ctx)
{
    if (!s_transport || !s_transport->close)
    {
        return;
    }

    s_transport->close(ctx);
}

void transport_destroy(void)
{
    if (s_transport && s_transport->destroy)
    {
        s_transport->destroy(s_transport);
    }

    s_transport = NULL;
}