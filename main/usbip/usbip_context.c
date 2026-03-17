/**
 * @file usbip_context.c
 * @brief USBIP context implementation
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbip/usbip_context.h"
#include "usbip/usbip_config.h"

#include <stdlib.h>
#include <string.h>

#define USBIP_RX_BUFFER_SIZE USBIP_MTU_SIZE

usbip_context_t *usbip_context_create(const transport_ops_t *ops, uint16_t port)
{
    usbip_context_t *ctx = NULL;

    ctx = (usbip_context_t *)calloc(1, sizeof(usbip_context_t));
    if (ctx == NULL)
    {
        return NULL;
    }

    /* Create transport context */
    ctx->transport = transport_create(ops, USBIP_RX_BUFFER_SIZE, port);
    if (ctx->transport == NULL)
    {
        free(ctx);
        return NULL;
    }

    ctx->rx_buffer = ctx->transport->rx_buffer;
    ctx->rx_buffer_size = ctx->transport->rx_buffer_size;
    ctx->state = USBIP_STATE_ACCEPTING;

    return ctx;
}

void usbip_context_destroy(usbip_context_t *ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    if (ctx->transport != NULL)
    {
        transport_destroy(ctx->transport);
        ctx->transport = NULL;
    }

    ctx->rx_buffer = NULL;
    free(ctx);
}

usbip_state_t usbip_context_get_state(usbip_context_t *ctx)
{
    if (ctx == NULL)
    {
        return USBIP_STATE_ACCEPTING;
    }

    return ctx->state;
}

void usbip_context_set_state(usbip_context_t *ctx, usbip_state_t state)
{
    if (ctx != NULL)
    {
        ctx->state = state;
    }
}

int usbip_context_init(usbip_context_t *ctx)
{
    if (ctx == NULL || ctx->transport == NULL)
    {
        return -1;
    }

    return transport_init(ctx->transport);
}

int usbip_context_accept(usbip_context_t *ctx)
{
    if (ctx == NULL || ctx->transport == NULL)
    {
        return -1;
    }

    return transport_accept(ctx->transport);
}

int usbip_context_send(usbip_context_t *ctx, const void *buf, size_t len)
{
    if (ctx == NULL || ctx->transport == NULL)
    {
        return -1;
    }

    return transport_send(ctx->transport, buf, len);
}

int usbip_context_recv(usbip_context_t *ctx, void *buf, size_t len)
{
    if (ctx == NULL || ctx->transport == NULL)
    {
        return -1;
    }

    return transport_recv(ctx->transport, buf, len);
}

int usbip_context_close(usbip_context_t *ctx)
{
    if (ctx == NULL || ctx->transport == NULL)
    {
        return -1;
    }

    return transport_close(ctx->transport);
}

void usbip_context_reset(usbip_context_t *ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->state = USBIP_STATE_ACCEPTING;
    usbip_context_close(ctx);
}