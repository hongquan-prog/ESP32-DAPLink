/**
 * @file usbip_server.c
 * @brief USBIP server legacy compatibility layer
 *
 * This file provides legacy API for backward compatibility.
 * New code should use usbip_protocol.h instead.
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbip/usbip_server.h"
#include "usbip/usbip_protocol.h"
#include "usbip/usbip_context.h"

#include <string.h>

/* Global state for legacy API compatibility */
static usbip_state_t s_usbip_state = USBIP_STATE_ACCEPTING;
static usbip_context_t *s_global_ctx = NULL;

void usbip_set_global_context(usbip_context_t *ctx)
{
    s_global_ctx = ctx;
}

usbip_context_t *usbip_get_global_context(void)
{
    return s_global_ctx;
}

usbip_state_t usbip_get_state(void)
{
    return s_usbip_state;
}

void usbip_set_state(usbip_state_t state)
{
    s_usbip_state = state;
}

int usbip_network_send(int s, const void *dataptr, size_t size, int flags)
{
    (void)s;
    (void)flags;

    if (s_global_ctx != NULL)
    {
        return usbip_context_send(s_global_ctx, dataptr, size);
    }

    return -1;
}