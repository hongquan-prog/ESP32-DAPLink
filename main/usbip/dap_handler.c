/**
 * @file dap_handler.c
 * @brief DAP endpoint handler implementation
 *
 * Handles DAP data processing for endpoint 1 (EP1).
 * Includes DAP response buffer management and signal handling.
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbip/dap_handler.h"
#include "usbip/usbip_endpoint_handler.h"
#include "usbip/usbip_protocol.h"
#include "usbip/usbip_config.h"
#include "usb_config.h"
#include "DAP.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/// Maximum Package Size for Command and Response data.
#if (USBIP_ENABLE_WINUSB == 1)
    #define USBIP_DAP_PACKET_SIZE 512U
#else
    #define USBIP_DAP_PACKET_SIZE 255U
#endif

/* DAP response buffer - stores the last processed response */
#if (USBIP_ENABLE_WINUSB == 1)
typedef struct
{
    uint32_t length;
    uint8_t buf[USBIP_DAP_PACKET_SIZE];
} dap_response_t;
#else
typedef struct
{
    uint8_t buf[USBIP_DAP_PACKET_SIZE];
} dap_response_t;
#endif

static dap_response_t s_dap_response;
static volatile bool s_response_ready = false;

/* EP1 DAP Handler callbacks */
static void dap_handle_out(usbip_context_t *ctx, usbip_stage2_header *header, uint32_t length);
static void dap_handle_in(usbip_context_t *ctx, usbip_stage2_header *header);
static void dap_handle_unlink(void);

static const usbip_endpoint_handler_t s_dap_handler = {
    .handle_out = dap_handle_out,
    .handle_in = dap_handle_in,
    .handle_unlink = dap_handle_unlink,
};

void dap_handler_init(void)
{
    memset(&s_dap_response, 0, sizeof(s_dap_response));
    s_response_ready = false;

    usbip_endpoint_handler_register(1, &s_dap_handler);
}

/* ========== DAP Response Buffer API ========== */

bool dap_is_response_ready(void)
{
    return s_response_ready;
}

bool dap_get_response(uint8_t **data, uint32_t *length)
{
    if (!s_response_ready)
    {
        return false;
    }

    *data = s_dap_response.buf;
#if (USBIP_ENABLE_WINUSB == 1)
    *length = s_dap_response.length;
#else
    *length = USBIP_DAP_PACKET_SIZE;
#endif

    s_response_ready = false;
    return true;
}

void dap_clear_response(void)
{
    s_response_ready = false;
}

/* ========== Endpoint Handler Implementation ========== */

static void dap_handle_out(usbip_context_t *ctx, usbip_stage2_header *header, uint32_t length)
{
    uint8_t *data_in = (uint8_t *)header;
    int res_length;

    (void)length;

    /* Skip USBIP header to get URB data */
    data_in = &(data_in[sizeof(usbip_stage2_header)]);

    /* Handle queued commands */
    if (data_in[0] == ID_DAP_QueueCommands)
    {
        data_in[0] = ID_DAP_ExecuteCommands;
    }

    /* Process DAP command synchronously */
    res_length = DAP_ProcessCommand(data_in, s_dap_response.buf);
    res_length &= 0xFFFF;

#if (USBIP_ENABLE_WINUSB == 1)
    s_dap_response.length = res_length;
#else
    (void)res_length; /* HID mode uses fixed packet size */
#endif

    s_response_ready = true;

    /* Send ACK to host */
    usbip_send_stage2_submit(ctx, header, 0, 0);
}

static void dap_handle_in(usbip_context_t *ctx, usbip_stage2_header *header)
{
    /* Not used in single-task architecture - fast_reply handles this */
    (void)ctx;
    (void)header;
}

static void dap_handle_unlink(void)
{
    /* Clear any pending response on unlink */
    s_response_ready = false;
}