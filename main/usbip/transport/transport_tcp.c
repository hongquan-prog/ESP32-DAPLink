/**
 * @file transport_tcp.c
 * @brief BSD Socket TCP transport implementation
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbip/transport/transport_tcp.h"
#include "usbip/transport/transport.h"
#include "usbip/usbip_config.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include <stdlib.h>
#include <string.h>

/**
 * @brief TCP private data structure
 */
typedef struct {
    int listen_sock;
    int client_sock;
#if USBIP_USE_IPV4
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
#else
    struct sockaddr_in6 server_addr;
    struct sockaddr_in6 client_addr;
#endif
} tcp_private_t;

static int tcp_init(transport_context_t *ctx)
{
    tcp_private_t *priv = NULL;
    int on = 1;

    /* Allocate private data */
    priv = (tcp_private_t *)calloc(1, sizeof(tcp_private_t));
    if (priv == NULL)
    {
        return -1;
    }
    ctx->private_data = priv;

    /* Initialize server address */
#if USBIP_USE_IPV4
    priv->server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    priv->server_addr.sin_family = AF_INET;
    priv->server_addr.sin_port = htons(ctx->port);
#else
    memset(&priv->server_addr.sin6_addr, 0, sizeof(priv->server_addr.sin6_addr));
    priv->server_addr.sin6_family = AF_INET6;
    priv->server_addr.sin6_port = htons(ctx->port);
#endif

    /* Create listening socket */
#if USBIP_USE_IPV4
    priv->listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
#else
    priv->listen_sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_IPV6);
#endif

    if (priv->listen_sock < 0)
    {
        free(priv);
        ctx->private_data = NULL;
        return -1;
    }

    /* Set socket options */
    setsockopt(priv->listen_sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&on, sizeof(on));
    setsockopt(priv->listen_sock, IPPROTO_TCP, TCP_NODELAY, (void *)&on, sizeof(on));

    /* Bind socket */
    if (bind(priv->listen_sock, (struct sockaddr *)&priv->server_addr,
             sizeof(priv->server_addr)) != 0)
    {
        close(priv->listen_sock);
        free(priv);
        ctx->private_data = NULL;
        return -1;
    }

    /* Listen for connections */
    if (listen(priv->listen_sock, 1) != 0)
    {
        close(priv->listen_sock);
        free(priv);
        ctx->private_data = NULL;
        return -1;
    }

    priv->client_sock = -1;

    return 0;
}

static int tcp_accept(transport_context_t *ctx)
{
    tcp_private_t *priv = (tcp_private_t *)ctx->private_data;
    socklen_t addr_len = sizeof(priv->client_addr);
    int on = 1;

    if (priv == NULL)
    {
        return -1;
    }

    /* Accept incoming connection */
    priv->client_sock = accept(priv->listen_sock,
                               (struct sockaddr *)&priv->client_addr,
                               &addr_len);

    if (priv->client_sock < 0)
    {
        return -1;
    }

    /* Set client socket options */
    setsockopt(priv->client_sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&on, sizeof(on));
    setsockopt(priv->client_sock, IPPROTO_TCP, TCP_NODELAY, (void *)&on, sizeof(on));

    return 0;
}

static int tcp_send(transport_context_t *ctx, const void *buf, size_t len)
{
    tcp_private_t *priv = (tcp_private_t *)ctx->private_data;

    if (priv == NULL || priv->client_sock < 0)
    {
        return -1;
    }

    return send(priv->client_sock, buf, len, 0);
}

static int tcp_recv(transport_context_t *ctx, void *buf, size_t len)
{
    tcp_private_t *priv = (tcp_private_t *)ctx->private_data;

    if (priv == NULL || priv->client_sock < 0)
    {
        return -1;
    }

    return recv(priv->client_sock, buf, len, 0);
}

static int tcp_close(transport_context_t *ctx)
{
    tcp_private_t *priv = (tcp_private_t *)ctx->private_data;

    if (priv == NULL)
    {
        return -1;
    }

    if (priv->client_sock >= 0)
    {
        close(priv->client_sock);
        priv->client_sock = -1;
    }

    return 0;
}

static void tcp_destroy(transport_context_t *ctx)
{
    tcp_private_t *priv = (tcp_private_t *)ctx->private_data;

    if (priv == NULL)
    {
        return;
    }

    /* Close client socket */
    if (priv->client_sock >= 0)
    {
        close(priv->client_sock);
    }

    /* Close listening socket */
    if (priv->listen_sock >= 0)
    {
        close(priv->listen_sock);
    }

    free(priv);
    ctx->private_data = NULL;
}

static const transport_ops_t s_tcp_ops = {
    .init = tcp_init,
    .accept = tcp_accept,
    .send = tcp_send,
    .recv = tcp_recv,
    .close = tcp_close,
    .destroy = tcp_destroy,
};

const transport_ops_t *transport_tcp_get_ops(void)
{
    return &s_tcp_ops;
}