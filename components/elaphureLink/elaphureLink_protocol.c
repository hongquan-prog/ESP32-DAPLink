/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-3-17     refactor     Code style compliance fixes
 */

#include "elaphureLink_protocol.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

extern int g_k_sock;
extern int usbip_network_send(int s, const void *dataptr, size_t size, int flags);
extern void malloc_dap_ringbuf(void);
extern void free_dap_ringbuf(void);
extern uint32_t DAP_ExecuteCommand(const uint8_t *request, uint8_t *response);

static uint8_t *s_el_process_buffer = NULL;

void el_process_buffer_malloc(void)
{
    if (s_el_process_buffer != NULL)
    {
        return;
    }

    free_dap_ringbuf();

    s_el_process_buffer = (uint8_t *)malloc(1500);
}

void el_process_buffer_free(void)
{
    if (s_el_process_buffer != NULL)
    {
        free(s_el_process_buffer);
        s_el_process_buffer = NULL;
    }
}

int el_handshake_process(int fd, void *buffer, size_t len)
{
    el_request_handshake_t *req = NULL;

    if (len != sizeof(el_request_handshake_t))
    {
        return -1;
    }

    req = (el_request_handshake_t *)buffer;
    if (ntohl(req->el_link_identifier) != EL_LINK_IDENTIFIER)
    {
        return -1;
    }

    if (ntohl(req->command) != EL_COMMAND_HANDSHAKE)
    {
        return -1;
    }

    el_response_handshake_t res;
    res.el_link_identifier = htonl(EL_LINK_IDENTIFIER);
    res.command = htonl(EL_COMMAND_HANDSHAKE);
    res.el_dap_version = htonl(EL_DAP_VERSION);
    usbip_network_send(fd, &res, sizeof(el_response_handshake_t), 0);

    return 0;
}

void el_dap_data_process(void *buffer, size_t len)
{
    int res = DAP_ExecuteCommand((const uint8_t *)buffer, s_el_process_buffer);

    res &= 0xFFFF;
    usbip_network_send(g_k_sock, s_el_process_buffer, res, 0);
}