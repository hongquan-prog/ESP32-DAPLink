/**
 * @file transport_kcp.c
 * @brief KCP (reliable UDP) transport implementation
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbip/transport/transport_kcp.h"
#include "usbip/transport/transport.h"
#include "usbip/usbip_config.h"

#include "components/kcp/ikcp.h"
#include "components/kcp/ikcp_util.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

/**
 * @brief KCP private data structure
 */
typedef struct {
    int udp_sock;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    ikcpcb *kcp;
    uint32_t conv;
    bool connected;
} kcp_private_t;

/**
 * @brief Set socket to non-blocking mode
 */
static void set_non_blocking(int sockfd)
{
    int flag = fcntl(sockfd, F_GETFL, 0);
    if (flag >= 0)
    {
        fcntl(sockfd, F_SETFL, flag | O_NONBLOCK);
    }
}

/**
 * @brief UDP output callback for KCP
 */
static int kcp_udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    kcp_private_t *priv = (kcp_private_t *)user;
    if (priv == NULL || priv->udp_sock < 0)
    {
        return -1;
    }

    int ret = sendto(priv->udp_sock, buf, len, 0,
                     (struct sockaddr *)&priv->client_addr,
                     sizeof(priv->client_addr));

    return (ret < 0) ? -1 : 0;
}

static int kcp_init(transport_context_t *ctx)
{
    kcp_private_t *priv = NULL;
    struct sockaddr_in server_addr;

    /* Allocate private data */
    priv = (kcp_private_t *)calloc(1, sizeof(kcp_private_t));
    if (priv == NULL)
    {
        return -1;
    }
    ctx->private_data = priv;
    priv->udp_sock = -1;
    priv->connected = false;

    /* Create UDP socket */
    priv->udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (priv->udp_sock < 0)
    {
        free(priv);
        ctx->private_data = NULL;
        return -1;
    }

    set_non_blocking(priv->udp_sock);

    /* Bind socket */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ctx->port);

    if (bind(priv->udp_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        close(priv->udp_sock);
        free(priv);
        ctx->private_data = NULL;
        return -1;
    }

    /* Create KCP control block */
    priv->conv = 1;
    priv->kcp = ikcp_create(priv->conv, priv);
    if (priv->kcp == NULL)
    {
        close(priv->udp_sock);
        free(priv);
        ctx->private_data = NULL;
        return -1;
    }

    priv->kcp->output = kcp_udp_output;
    ikcp_wndsize(priv->kcp, 4096, 4096);
    ikcp_nodelay(priv->kcp, 2, 2, 2, 1);
    priv->kcp->interval = 0;
    priv->kcp->rx_minrto = 1;
    priv->kcp->fastresend = 1;
    ikcp_setmtu(priv->kcp, 768);

    return 0;
}

static int kcp_accept(transport_context_t *ctx)
{
    kcp_private_t *priv = (kcp_private_t *)ctx->private_data;
    uint8_t temp_buf[64];
    ssize_t ret;

    if (priv == NULL || priv->udp_sock < 0)
    {
        return -1;
    }

    /* For KCP, accept means waiting for first packet from client */
    priv->client_addr_len = sizeof(priv->client_addr);

    while (!priv->connected)
    {
        ret = recvfrom(priv->udp_sock, temp_buf, sizeof(temp_buf), 0,
                       (struct sockaddr *)&priv->client_addr,
                       &priv->client_addr_len);

        if (ret > 0)
        {
            /* Feed to KCP */
            ikcp_input(priv->kcp, (const char *)temp_buf, ret);
            priv->connected = true;
            return 0;
        }

        /* Small delay to avoid busy loop */
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return 0;
}

static int kcp_send(transport_context_t *ctx, const void *buf, size_t len)
{
    kcp_private_t *priv = (kcp_private_t *)ctx->private_data;

    if (priv == NULL || priv->kcp == NULL || !priv->connected)
    {
        return -1;
    }

    int ret = ikcp_send(priv->kcp, (const char *)buf, len);
    if (ret < 0)
    {
        return -1;
    }

    ikcp_flush(priv->kcp);

    return (int)len;
}

static int kcp_recv(transport_context_t *ctx, void *buf, size_t len)
{
    kcp_private_t *priv = (kcp_private_t *)ctx->private_data;
    ssize_t ret;
    uint8_t temp_buf[USBIP_MTU_SIZE];

    if (priv == NULL || priv->kcp == NULL || priv->udp_sock < 0)
    {
        return -1;
    }

    /* Update KCP */
    ikcp_update(priv->kcp, iclock());

    /* Receive from UDP socket and feed to KCP */
    while (1)
    {
        socklen_t addr_len = sizeof(priv->client_addr);
        ret = recvfrom(priv->udp_sock, temp_buf, sizeof(temp_buf), 0,
                       (struct sockaddr *)&priv->client_addr, &addr_len);

        if (ret <= 0)
        {
            break;
        }

        ikcp_input(priv->kcp, (const char *)temp_buf, ret);
    }

    /* Receive from KCP */
    ret = ikcp_recv(priv->kcp, (char *)buf, len);

    return (int)ret;
}

static int kcp_close(transport_context_t *ctx)
{
    kcp_private_t *priv = (kcp_private_t *)ctx->private_data;

    if (priv == NULL)
    {
        return -1;
    }

    /* Reset KCP state */
    if (priv->kcp != NULL)
    {
        ikcp_flush(priv->kcp);
    }

    priv->connected = false;
    priv->client_addr_len = 0;
    memset(&priv->client_addr, 0, sizeof(priv->client_addr));

    return 0;
}

static void kcp_destroy(transport_context_t *ctx)
{
    kcp_private_t *priv = (kcp_private_t *)ctx->private_data;

    if (priv == NULL)
    {
        return;
    }

    if (priv->kcp != NULL)
    {
        ikcp_release(priv->kcp);
        priv->kcp = NULL;
    }

    if (priv->udp_sock >= 0)
    {
        close(priv->udp_sock);
        priv->udp_sock = -1;
    }

    free(priv);
    ctx->private_data = NULL;
}

static const transport_ops_t s_kcp_ops = {
    .init = kcp_init,
    .accept = kcp_accept,
    .send = kcp_send,
    .recv = kcp_recv,
    .close = kcp_close,
    .destroy = kcp_destroy,
};

const transport_ops_t *transport_kcp_get_ops(void)
{
    return &s_kcp_ops;
}