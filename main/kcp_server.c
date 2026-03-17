/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-10-08    windows       Initial version
 * 2026-03-17    lihongquan    Refactored according to code style guide
 */

#include "main/kcp_server.h"
#include "main/usbip_server.h"
#include "main/wifi_configuration.h"
#include "main/DAP_handle.h"

#include "components/kcp/ikcp.h"
#include "components/kcp/ikcp_util.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

extern int kSock;

/* Static buffer for KCP data (>64 bytes, use static to avoid stack overflow) */
static char s_kcp_buffer[MTU_SIZE];
static struct sockaddr_in s_client_addr = { 0 };
static ikcpcb *s_kcp = NULL;

/**
 * @brief Set socket to non-blocking mode
 *
 * @param sockfd Socket file descriptor
 */
static void set_non_blocking(int sockfd)
{
    int flag = fcntl(sockfd, F_GETFL, 0);

    if (flag < 0)
    {
        os_printf("fcntl F_GETFL fail\n");

        return;
    }

    if (fcntl(sockfd, F_SETFL, flag | O_NONBLOCK) < 0)
    {
        os_printf("fcntl F_SETFL fail\n");
    }
}

/**
 * @brief UDP output callback for KCP
 *
 * @param buf Data buffer to send
 * @param len Buffer length
 * @param kcp KCP control block
 * @param user User data pointer
 * @return 0 on success
 */
static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    int ret = -1;
    int delay_time = 10;
    int errcode = 0;

    /* Unfortunately, esp8266 often fails due to lack of memory */
    while (ret < 0)
    {
        ret = sendto(kSock, buf, len, 0, (struct sockaddr *)&s_client_addr, sizeof(s_client_addr));
        if (ret < 0)
        {
            errcode = errno;
            if (errno != ENOMEM)
            {
                os_printf("unknown errcode %d\r\n", errcode);
            }

            vTaskDelay(pdMS_TO_TICKS(delay_time));
            delay_time += 10;
        }
    }

    return 0;
}

int kcp_network_send(const char *buffer, int len)
{
    ikcp_send(s_kcp, buffer, len);
    ikcp_flush(s_kcp);

    return 0;
}

void kcp_server_task(void)
{
    TickType_t x_last_wake_time = 0;
    struct sockaddr_in server_addr;
    socklen_t socklen = 0;
    int err = 0;
    int ret = -1;

    /* 初始化 */
    x_last_wake_time = xTaskGetTickCount();
    /* 主循环 */
    while (1)
    {
        /* 创建UDP套接字 */
        kSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (kSock < 0)
        {
            os_printf("Unable to create socket: errno %d", errno);
            break;
        }
        os_printf("Socket created\r\n");
        set_non_blocking(kSock);

        /* 绑定套接字到本地端口 */
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(PORT);
        socklen = sizeof(s_client_addr);
        err = bind(kSock, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (err < 0)
        {
            os_printf("Socket unable to bind: errno %d\r\n", errno);
        }

        os_printf("Socket binded\r\n");

        /* 初始化KCP控制块 */
        if (s_kcp == NULL)
        {
            s_kcp = ikcp_create(1, (void *)0);
        }

        if (s_kcp == NULL)
        {
            os_printf("can not create kcp control block\r\n");
            break;
        }

        s_kcp->output = udp_output;
        ikcp_wndsize(s_kcp, 4096, 4096);

        /* 设置KCP为快速模式 */
        ikcp_nodelay(s_kcp, 2, 2, 2, 1);
        s_kcp->interval = 0;
        s_kcp->rx_minrto = 1;
        s_kcp->fastresend = 1;
        ikcp_setmtu(s_kcp, 768);

        /* KCP任务主循环 */
        while (1)
        {
            /* 睡眠精确时间 */
            vTaskDelayUntil(&x_last_wake_time, pdMS_TO_TICKS(1));
            ikcp_update(s_kcp, iclock());

            /* 从UDP接收数据 */
            while (1)
            {
                ret = recvfrom(kSock, s_kcp_buffer, MTU_SIZE, 0, (struct sockaddr *)&s_client_addr, &socklen);
                if (ret < 0)
                {
                    break;
                }

                ikcp_input(s_kcp, s_kcp_buffer, ret);
            }

            /* 从KCP接收数据 */
            while (1)
            {
                ret = ikcp_recv(s_kcp, s_kcp_buffer, MTU_SIZE);
                if (ret < 0)
                {
                    break;
                }

                /* 接收用户数据并处理 */
                switch (usbip_get_state())
                {
                case USBIP_STATE_EMULATING:
                    usbip_emulate((uint8_t *)s_kcp_buffer, ret);
                    break;

                case USBIP_STATE_ACCEPTING:
                    usbip_set_state(USBIP_STATE_ATTACHING);
                    /* fallthrough */

                case USBIP_STATE_ATTACHING:
                    usbip_attach((uint8_t *)s_kcp_buffer, ret);
                    break;

                default:
                    os_printf("unkonw kstate!\r\n");
                }
            }
        }

        /* 清理资源 */
        if (s_kcp)
        {
            ikcp_release(s_kcp);
        }

        if (kSock != -1)
        {
            os_printf("Shutting down socket and restarting...\r\n");
            shutdown(kSock, 0);
            close(kSock);
            if (usbip_get_state() == USBIP_STATE_EMULATING)
            {
                usbip_set_state(USBIP_STATE_ACCEPTING);
            }

            /* 重启DAP Handle */
            dap_set_restart_signal(DAP_SIGNAL_RESET);
            dap_notify_task();
        }
    }

    vTaskDelete(NULL);
}