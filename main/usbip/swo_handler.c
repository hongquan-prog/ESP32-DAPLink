/**
 * @file swo_handler.c
 * @brief SWO Trace endpoint handler implementation
 *
 * Handles SWO trace data for endpoint 2 (EP2).
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbip/swo_handler.h"
#include "usbip/usbip_endpoint_handler.h"
#include "usbip/usbip_protocol.h"
#include "usbip/usbip_config.h"
#include "DAP.h"

#include <stdint.h>

/* SWO Trace data */
static uint8_t *s_swo_data_to_send = NULL;
static uint32_t s_swo_data_num = 0;

/* EP2 SWO Handler callbacks */
static void swo_handle_out(usbip_context_t *ctx, usbip_stage2_header *header, uint32_t length);
static void swo_handle_in(usbip_context_t *ctx, usbip_stage2_header *header);

static const usbip_endpoint_handler_t s_swo_handler = {
    .handle_out = swo_handle_out,
    .handle_in = swo_handle_in,
    .handle_unlink = NULL,
};

void swo_handler_init(void)
{
    s_swo_data_to_send = NULL;
    s_swo_data_num = 0;

    usbip_endpoint_handler_register(2, &s_swo_handler);
}

/* ========== SWO Data API ========== */

void SWO_QueueTransfer(uint8_t *buf, uint32_t num)
{
    s_swo_data_to_send = buf;
    s_swo_data_num = num;
}

/* ========== Endpoint Handler Implementation ========== */

static void swo_handle_out(usbip_context_t *ctx, usbip_stage2_header *header, uint32_t length)
{
    (void)length;
    usbip_send_stage2_submit(ctx, header, 0, 0);
}

static void swo_handle_in(usbip_context_t *ctx, usbip_stage2_header *header)
{
#if (SWO_FUNCTION_ENABLE == 1)
    if (kSwoTransferBusy)
    {
        usbip_send_stage2_submit_data(ctx, header, 0, (void *)s_swo_data_to_send, s_swo_data_num);
        SWO_TransferComplete();
    }
    else
    {
        usbip_send_stage2_submit(ctx, header, 0, 0);
    }
#else
    usbip_send_stage2_submit(ctx, header, 0, 0);
#endif
}