/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-3-17     refactor     Code style compliance fixes
 * 2026-3-17     refactor     Remove upper layer dependency, output via parameters
 */

#include "elaphureLink_protocol.h"
#include "DAP.h"

#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

static uint8_t *s_el_process_buffer = NULL;

void el_process_buffer_malloc(void)
{
    if (s_el_process_buffer != NULL)
    {
        return;
    }

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

bool el_is_handshake_request(const void *buffer, size_t len)
{
    const el_request_handshake_t *req = (const el_request_handshake_t *)buffer;

    if (len != sizeof(el_request_handshake_t))
    {
        return false;
    }

    if (ntohl(req->el_link_identifier) != EL_LINK_IDENTIFIER)
    {
        return false;
    }

    if (ntohl(req->command) != EL_COMMAND_HANDSHAKE)
    {
        return false;
    }

    return true;
}

size_t el_create_handshake_response(el_response_handshake_t *response)
{
    response->el_link_identifier = htonl(EL_LINK_IDENTIFIER);
    response->command = htonl(EL_COMMAND_HANDSHAKE);
    response->el_dap_version = htonl(EL_DAP_VERSION);

    return sizeof(el_response_handshake_t);
}

void el_process_dap_command(const void *buffer, size_t len, uint8_t *response, size_t *response_size)
{
    int res;

    (void)len;

    if (s_el_process_buffer == NULL)
    {
        *response_size = 0;
        return;
    }

    res = DAP_ExecuteCommand((const uint8_t *)buffer, s_el_process_buffer);
    res &= 0xFFFF;

    memcpy(response, s_el_process_buffer, res);
    *response_size = res;
}