/**
 * @file transport_netconn.c
 * @brief LwIP Netconn transport implementation
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbip/transport/transport_netconn.h"
#include "usbip/transport/transport.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netbuf.h"
#include "lwip/api.h"

#include <stdlib.h>
#include <string.h>

/**
 * @brief Netconn private data structure
 */
typedef struct {
    struct netconn *listen_nc;
    struct netconn *client_nc;
    struct netbuf *rx_netbuf;
    uint8_t *rx_ptr;
    uint16_t rx_len;
} netconn_private_t;

static int _netconn_init(transport_context_t *ctx)
{
    netconn_private_t *priv = NULL;

    /* Allocate private data */
    priv = (netconn_private_t *)calloc(1, sizeof(netconn_private_t));
    if (priv == NULL)
    {
        return -1;
    }
    ctx->private_data = priv;

    /* Create netconn */
    priv->listen_nc = netconn_new(NETCONN_TCP);
    if (priv->listen_nc == NULL)
    {
        free(priv);
        ctx->private_data = NULL;
        return -1;
    }

    /* Set non-blocking */
    netconn_set_nonblocking(priv->listen_nc, NETCONN_FLAG_NON_BLOCKING);

    /* Bind */
    if (netconn_bind(priv->listen_nc, IP_ADDR_ANY, ctx->port) != ERR_OK)
    {
        netconn_delete(priv->listen_nc);
        free(priv);
        ctx->private_data = NULL;
        return -1;
    }

    /* Listen */
    if (netconn_listen(priv->listen_nc) != ERR_OK)
    {
        netconn_delete(priv->listen_nc);
        free(priv);
        ctx->private_data = NULL;
        return -1;
    }

    return 0;
}

static int _netconn_accept(transport_context_t *ctx)
{
    netconn_private_t *priv = (netconn_private_t *)ctx->private_data;
    err_t err;

    if (priv == NULL)
    {
        return -1;
    }

    /* Accept connection */
    err = netconn_accept(priv->listen_nc, &priv->client_nc);
    if (err != ERR_OK)
    {
        if (priv->client_nc != NULL)
        {
            netconn_delete(priv->client_nc);
            priv->client_nc = NULL;
        }
        return -1;
    }

    return 0;
}

static int _netconn_send(transport_context_t *ctx, const void *buf, size_t len)
{
    netconn_private_t *priv = (netconn_private_t *)ctx->private_data;

    if (priv == NULL || priv->client_nc == NULL)
    {
        return -1;
    }

    err_t err = netconn_write(priv->client_nc, buf, len, NETCONN_COPY);
    if (err != ERR_OK)
    {
        return -1;
    }

    return (int)len;
}

static int _netconn_recv(transport_context_t *ctx, void *buf, size_t len)
{
    netconn_private_t *priv = (netconn_private_t *)ctx->private_data;

    if (priv == NULL || priv->client_nc == NULL)
    {
        return -1;
    }

    /* If we have remaining data in netbuf, copy it first */
    if (priv->rx_netbuf != NULL && priv->rx_ptr != NULL && priv->rx_len > 0)
    {
        size_t copy_len = (len < priv->rx_len) ? len : priv->rx_len;
        memcpy(buf, priv->rx_ptr, copy_len);
        priv->rx_ptr += copy_len;
        priv->rx_len -= copy_len;

        if (priv->rx_len == 0)
        {
            netbuf_delete(priv->rx_netbuf);
            priv->rx_netbuf = NULL;
            priv->rx_ptr = NULL;
        }

        return (int)copy_len;
    }

    /* Receive new netbuf */
    struct netbuf *netbuf = NULL;
    err_t err = netconn_recv(priv->client_nc, &netbuf);

    if (err != ERR_OK)
    {
        if (netbuf != NULL)
        {
            netbuf_delete(netbuf);
        }
        return (err == ERR_CLSD) ? 0 : -1;
    }

    /* Get data pointer */
    uint8_t *data = NULL;
    uint16_t data_len = 0;
    netbuf_data(netbuf, (void **)&data, &data_len);

    if (data_len == 0)
    {
        netbuf_delete(netbuf);
        return 0;
    }

    /* Copy data */
    size_t copy_len = (len < data_len) ? len : data_len;
    memcpy(buf, data, copy_len);

    /* Store remaining data if any */
    if (data_len > copy_len)
    {
        priv->rx_netbuf = netbuf;
        priv->rx_ptr = data + copy_len;
        priv->rx_len = data_len - copy_len;
    }
    else
    {
        netbuf_delete(netbuf);
    }

    return (int)copy_len;
}

static int _netconn_close(transport_context_t *ctx)
{
    netconn_private_t *priv = (netconn_private_t *)ctx->private_data;

    if (priv == NULL)
    {
        return -1;
    }

    /* Clean up receive buffer */
    if (priv->rx_netbuf != NULL)
    {
        netbuf_delete(priv->rx_netbuf);
        priv->rx_netbuf = NULL;
        priv->rx_ptr = NULL;
        priv->rx_len = 0;
    }

    /* Close client connection */
    if (priv->client_nc != NULL)
    {
        priv->client_nc->pending_err = ERR_CLSD;
        netconn_close(priv->client_nc);
        netconn_delete(priv->client_nc);
        priv->client_nc = NULL;
    }

    return 0;
}

static void _netconn_destroy(transport_context_t *ctx)
{
    netconn_private_t *priv = (netconn_private_t *)ctx->private_data;

    if (priv == NULL)
    {
        return;
    }

    /* Clean up receive buffer */
    if (priv->rx_netbuf != NULL)
    {
        netbuf_delete(priv->rx_netbuf);
    }

    /* Close client connection */
    if (priv->client_nc != NULL)
    {
        netconn_close(priv->client_nc);
        netconn_delete(priv->client_nc);
    }

    /* Close listening connection */
    if (priv->listen_nc != NULL)
    {
        netconn_close(priv->listen_nc);
        netconn_delete(priv->listen_nc);
    }

    free(priv);
    ctx->private_data = NULL;
}

static const transport_ops_t s_netconn_ops = {
    .init = _netconn_init,
    .accept = _netconn_accept,
    .send = _netconn_send,
    .recv = _netconn_recv,
    .close = _netconn_close,
    .destroy = _netconn_destroy,
};

const transport_ops_t *transport_netconn_get_ops(void)
{
    return &s_netconn_ops;
}