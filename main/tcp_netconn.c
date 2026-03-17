/*
 * Copyright (c) 2015, SuperHouse Automation Pty Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Change Logs:
 * Date           Author           Notes
 * 2015           SuperHouse       Initial version
 * 2026-03-17     lihongquan       Refactored according to code style guide
 */

#include "main/tcp_netconn.h"
#include "main/usbip_server.h"
#include "main/wifi_configuration.h"
#include "main/DAP_handle.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netbuf.h"
#include "lwip/api.h"
#include "lwip/tcp.h"
#include <lwip/netdb.h>

#include <string.h>
#include <stdint.h>
#include <sys/param.h>

#define PORT                        3240
#define EVENTS_QUEUE_SIZE           50

#ifdef CALLBACK_DEBUG
#define debug(s, ...) os_printf("%s: " s "\n", "Cb:", ##__VA_ARGS__)
#else
#define debug(s, ...)
#endif

typedef struct
{
    struct netconn *nc;
    uint8_t type;
} netconn_event_t;

static QueueHandle_t s_queue_events = NULL;
static struct netconn *s_netconn = NULL;

/**
 * @brief Send data through TCP netconn connection
 *
 * @param buffer Pointer to data buffer
 * @param len Length of data to send
 * @return 0 on success, negative value on error
 */
int tcp_netconn_send(const void *buffer, size_t len)
{
    return netconn_write(s_netconn, buffer, len, NETCONN_COPY);
}

/**
 * @brief Netconn callback function
 *
 * This function will be called in LwIP in each event on netconn
 *
 * @param conn Netconn connection
 * @param evt Event type
 * @param length Data length
 */
static void net_callback(struct netconn *conn, enum netconn_evt evt, uint16_t length)
{
    netconn_event_t event = {0};

    /* Show some callback information (debug) */
    debug("sock:%u\tsta:%u\tevt:%u\tlen:%u\ttyp:%u\tfla:%02x\terr:%d",
          (uint32_t)conn, conn->state, evt, length, conn->type, conn->flags, conn->pending_err);

    /* If netconn got error, it is close or deleted, dont do treatments on it */
    if (conn->pending_err)
    {
        return;
    }

    /* Treatments only on rcv events */
    switch (evt)
    {
    case NETCONN_EVT_RCVPLUS:
        event.nc = conn;
        event.type = evt;
        break;

    default:
        return;
    }

    /* Send the event to the queue */
    xQueueSend(s_queue_events, &event, 1000);
}

/**
 * @brief Initialize a server netconn and listen port
 *
 * @param nc Pointer to netconn pointer
 * @param port Port number to listen
 * @param callback Callback function
 */
static void set_tcp_server_netconn(struct netconn **nc, uint16_t port, netconn_callback callback)
{
    if (nc == NULL)
    {
        os_printf("%s: netconn missing.\n", __FUNCTION__);

        return;
    }

    *nc = netconn_new_with_callback(NETCONN_TCP, net_callback);
    if (!*nc)
    {
        os_printf("Status monitor: Failed to allocate netconn.\n");

        return;
    }

    netconn_set_nonblocking(*nc, NETCONN_FLAG_NON_BLOCKING);
    netconn_bind(*nc, IP_ADDR_ANY, port);
    netconn_listen(*nc);
}

/**
 * @brief Close and delete a socket properly
 *
 * @param nc Netconn connection to close
 */
static void close_tcp_netconn(struct netconn *nc)
{
    /* It is hacky way to be sure than callback will don't do treatment on a netconn closed and deleted */
    nc->pending_err = ERR_CLSD;
    netconn_close(nc);
    netconn_delete(nc);
}

/**
 * @brief TCP netconn server main task
 */
void tcp_netconn_task(void)
{
    struct netconn *nc = NULL;
    struct netconn *nc_in = NULL;
    struct netbuf *netbuf = NULL;
    netconn_event_t event = {0};
    char *buffer = NULL;
    uint16_t len_buf = 0;
    int err = 0;
    ip_addr_t client_addr = {0};
    uint16_t client_port = 0;

    s_queue_events = xQueueCreate(EVENTS_QUEUE_SIZE, sizeof(netconn_event_t));
    set_tcp_server_netconn(&nc, PORT, net_callback);
    os_printf("Server netconn %u ready on port %u.\n", (uint32_t)nc, PORT);

    while (1)
    {
        /* Wait here an event on netconn */
        xQueueReceive(s_queue_events, &event, portMAX_DELAY);

        /* If netconn is a server and receive incoming event on it */
        if (event.nc->state == NETCONN_LISTEN)
        {
            os_printf("Client incoming on server %u.\n", (uint32_t)event.nc);
            err = netconn_accept(event.nc, &nc_in);
            if (err != ERR_OK)
            {
                if (nc_in)
                {
                    netconn_delete(nc_in);
                }
            }

            os_printf("New client is %u.\n", (uint32_t)nc_in);
            netconn_peer(nc_in, &client_addr, &client_port);
        }
        /* If netconn is the client and receive data */
        else if (event.nc->state != NETCONN_LISTEN)
        {
            /* Data incoming */
            if (netconn_recv(event.nc, &netbuf) == ERR_OK)
            {
                do
                {
                    netbuf_data(netbuf, (void *)&buffer, &len_buf);
                    s_netconn = event.nc;

                    switch (usbip_get_state())
                    {
                    case USBIP_STATE_ACCEPTING:
                        usbip_set_state(USBIP_STATE_ATTACHING);
                        /* fallthrough */

                    case USBIP_STATE_ATTACHING:
                        usbip_attach((uint8_t *)buffer, len_buf);
                        usbip_set_state(USBIP_STATE_EMULATING);
                        break;

                    case USBIP_STATE_EMULATING:
                        usbip_emulate((uint8_t *)buffer, len_buf);
                        break;

                    default:
                        os_printf("unkonw kstate!\r\n");
                    }
                } while (netbuf_next(netbuf) >= 0);

                netbuf_delete(netbuf);
            }
            else
            {
                if (event.nc->pending_err == ERR_CLSD)
                {
                    /* The same hacky way to treat a closed connection */
                    continue;
                }

                os_printf("Shutting down socket and restarting...\r\n");
                close_tcp_netconn(event.nc);

                if (usbip_get_state() == USBIP_STATE_EMULATING)
                {
                    usbip_set_state(USBIP_STATE_ACCEPTING);
                }

                /* Restart DAP Handle */
                dap_set_restart_signal(DAP_SIGNAL_RESET);
                dap_notify_task();
            }
        }
    }
}