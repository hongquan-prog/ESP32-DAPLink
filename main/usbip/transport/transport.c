/**
 * @file transport.c
 * @brief Transport context management implementation
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbip/transport/transport.h"
#include <stdlib.h>
#include <string.h>

transport_context_t *transport_create(const transport_ops_t *ops,
                                       size_t rx_buffer_size,
                                       uint16_t port)
{
    if (ops == NULL)
    {
        return NULL;
    }

    transport_context_t *ctx = (transport_context_t *)calloc(1, sizeof(transport_context_t));
    if (ctx == NULL)
    {
        return NULL;
    }

    ctx->ops = ops;
    ctx->port = port;
    ctx->state = TRANSPORT_STATE_DISCONNECTED;

    /* Allocate receive buffer */
    if (rx_buffer_size > 0)
    {
        ctx->rx_buffer = (uint8_t *)malloc(rx_buffer_size);
        if (ctx->rx_buffer == NULL)
        {
            free(ctx);
            return NULL;
        }
        ctx->rx_buffer_size = rx_buffer_size;
    }

    return ctx;
}

int transport_init(transport_context_t *ctx)
{
    if (ctx == NULL || ctx->ops == NULL || ctx->ops->init == NULL)
    {
        return -1;
    }

    int ret = ctx->ops->init(ctx);
    if (ret == 0)
    {
        ctx->state = TRANSPORT_STATE_LISTENING;
    }

    return ret;
}

int transport_accept(transport_context_t *ctx)
{
    if (ctx == NULL || ctx->ops == NULL || ctx->ops->accept == NULL)
    {
        return -1;
    }

    int ret = ctx->ops->accept(ctx);
    if (ret == 0)
    {
        ctx->state = TRANSPORT_STATE_CONNECTED;
    }

    return ret;
}

int transport_send(transport_context_t *ctx, const void *buf, size_t len)
{
    if (ctx == NULL || ctx->ops == NULL || ctx->ops->send == NULL)
    {
        return -1;
    }

    return ctx->ops->send(ctx, buf, len);
}

int transport_recv(transport_context_t *ctx, void *buf, size_t len)
{
    if (ctx == NULL || ctx->ops == NULL || ctx->ops->recv == NULL)
    {
        return -1;
    }

    /* If no buffer provided, use internal buffer */
    if (buf == NULL && ctx->rx_buffer != NULL)
    {
        return ctx->ops->recv(ctx, ctx->rx_buffer, ctx->rx_buffer_size);
    }

    return ctx->ops->recv(ctx, buf, len);
}

uint8_t *transport_get_rx_buffer(transport_context_t *ctx)
{
    if (ctx == NULL)
    {
        return NULL;
    }

    return ctx->rx_buffer;
}

int transport_close(transport_context_t *ctx)
{
    if (ctx == NULL || ctx->ops == NULL || ctx->ops->close == NULL)
    {
        return -1;
    }

    int ret = ctx->ops->close(ctx);
    ctx->state = TRANSPORT_STATE_LISTENING;

    return ret;
}

void transport_destroy(transport_context_t *ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    if (ctx->ops != NULL && ctx->ops->destroy != NULL)
    {
        ctx->ops->destroy(ctx);
    }

    if (ctx->rx_buffer != NULL)
    {
        free(ctx->rx_buffer);
        ctx->rx_buffer = NULL;
    }

    free(ctx);
}

transport_state_t transport_get_state(transport_context_t *ctx)
{
    if (ctx == NULL)
    {
        return TRANSPORT_STATE_DISCONNECTED;
    }

    return ctx->state;
}