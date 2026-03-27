/**
 * @file usbip_endpoint_handler.c
 * @brief USBIP endpoint handler implementation
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbip/usbip_endpoint_handler.h"
#include <string.h>

#define MAX_ENDPOINT_HANDLERS 16

typedef struct
{
    uint8_t endpoint;
    const usbip_endpoint_handler_t *handler;
} endpoint_handler_entry_t;

static endpoint_handler_entry_t s_handlers[MAX_ENDPOINT_HANDLERS];
static uint8_t s_handler_count = 0;

void usbip_endpoint_handler_register(uint8_t endpoint, const usbip_endpoint_handler_t *handler)
{
    if (s_handler_count >= MAX_ENDPOINT_HANDLERS)
    {
        return;
    }

    /* Check if endpoint already registered */
    for (uint8_t i = 0; i < s_handler_count; i++)
    {
        if (s_handlers[i].endpoint == endpoint)
        {
            s_handlers[i].handler = handler;
            return;
        }
    }

    /* Add new entry */
    s_handlers[s_handler_count].endpoint = endpoint;
    s_handlers[s_handler_count].handler = handler;
    s_handler_count++;
}

const usbip_endpoint_handler_t *usbip_endpoint_handler_get(uint8_t endpoint)
{
    for (uint8_t i = 0; i < s_handler_count; i++)
    {
        if (s_handlers[i].endpoint == endpoint)
        {
            return s_handlers[i].handler;
        }
    }

    return NULL;
}