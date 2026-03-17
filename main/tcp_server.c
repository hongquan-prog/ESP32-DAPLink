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

#include "main/tcp_server.h"
#include "main/wifi_configuration.h"
#include "main/usbip_server.h"
#include "DAP_handle.h"

#include "components/elaphureLink/elaphureLink_protocol.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include <string.h>
#include <stdint.h>
#include <sys/param.h>

int kSock = -1;

/**
 * @brief TCP server main task
 *
 * @param pvParameters Task parameters (unused)
 */
void tcp_server_task(void *pvParameters)
{
    uint8_t tcp_rx_buffer[1500];
    char addr_str[128];
    int addr_family = 0;
    int ip_protocol = 0;
    int on = 1;
    int listen_sock = 0;
    int err = 0;
    int len = 0;
    uint32_t addr_len = 0;

    while (1)
    {
#ifdef CONFIG_EXAMPLE_IPV4
        struct sockaddr_in dest_addr;

        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
#else
        struct sockaddr_in6 dest_addr;

        memset(&dest_addr.sin6_addr.un, 0, sizeof(dest_addr.sin6_addr.un));
        dest_addr.sin6_family = AF_INET6;
        dest_addr.sin6_port = htons(PORT);
        addr_family = AF_INET6;
        ip_protocol = IPPROTO_IPV6;
        inet6_ntoa_r(dest_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
#endif

        /* Create socket */
        listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (listen_sock < 0)
        {
            os_printf("Unable to create socket: errno %d\r\n", errno);
            break;
        }

        os_printf("Socket created\r\n");
        setsockopt(listen_sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&on, sizeof(on));
        setsockopt(listen_sock, IPPROTO_TCP, TCP_NODELAY, (void *)&on, sizeof(on));

        /* Bind socket */
        err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0)
        {
            os_printf("Socket unable to bind: errno %d\r\n", errno);
            break;
        }

        os_printf("Socket binded\r\n");

        /* Listen for connections */
        err = listen(listen_sock, 1);
        if (err != 0)
        {
            os_printf("Error occured during listen: errno %d\r\n", errno);
            break;
        }

        os_printf("Socket listening\r\n");

#ifdef CONFIG_EXAMPLE_IPV6
        struct sockaddr_in6 source_addr;
#else
        struct sockaddr_in source_addr;
#endif

        addr_len = sizeof(source_addr);

        while (1)
        {
            /* Accept incoming connection */
            kSock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
            if (kSock < 0)
            {
                os_printf("Unable to accept connection: errno %d\r\n", errno);
                break;
            }

            setsockopt(kSock, SOL_SOCKET, SO_KEEPALIVE, (void *)&on, sizeof(on));
            setsockopt(kSock, IPPROTO_TCP, TCP_NODELAY, (void *)&on, sizeof(on));
            os_printf("Socket accepted\r\n");

            while (1)
            {
                len = recv(kSock, tcp_rx_buffer, sizeof(tcp_rx_buffer), 0);

                /* Error occured during receiving */
                if (len < 0)
                {
                    os_printf("recv failed: errno %d\r\n", errno);
                    break;
                }
                /* Connection closed */
                else if (len == 0)
                {
                    os_printf("Connection closed\r\n");
                    break;
                }
                /* Data received */
                else
                {
                    switch (usbip_get_state())
                    {
                    case USBIP_STATE_ACCEPTING:
                        usbip_set_state(USBIP_STATE_ATTACHING);
                        /* fallthrough */

                    case USBIP_STATE_ATTACHING:
                        /* elaphureLink handshake */
                        if (el_handshake_process(kSock, tcp_rx_buffer, len) == 0)
                        {
                            /* handshake successed */
                            usbip_set_state(USBIP_STATE_EL_DATA_PHASE);
                            dap_set_restart_signal(DAP_SIGNAL_DELETE);
                            el_process_buffer_malloc();
                            break;
                        }

                        usbip_attach(tcp_rx_buffer, len);
                        break;

                    case USBIP_STATE_EMULATING:
                        usbip_emulate(tcp_rx_buffer, len);
                        break;

                    case USBIP_STATE_EL_DATA_PHASE:
                        el_dap_data_process(tcp_rx_buffer, len);
                        break;

                    default:
                        os_printf("unkonw kstate!\r\n");
                    }
                }
            }

            /* Clean up connection */
            if (kSock != -1)
            {
                os_printf("Shutting down socket and restarting...\r\n");
                close(kSock);

                usbip_state_t state = usbip_get_state();
                if (state == USBIP_STATE_EMULATING || state == USBIP_STATE_EL_DATA_PHASE)
                {
                    usbip_set_state(USBIP_STATE_ACCEPTING);
                }

                /* Restart DAP Handle */
                el_process_buffer_free();
                dap_set_restart_signal(DAP_SIGNAL_RESET);
                dap_notify_task();
            }
        }
    }

    vTaskDelete(NULL);
}