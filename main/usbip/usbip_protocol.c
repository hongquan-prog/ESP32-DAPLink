/**
 * @file usbip_protocol.c
 * @brief USBIP protocol utility functions implementation
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbip/usbip_protocol.h"
#include "usbip/dap_handler.h"

#include <string.h>
#include <arpa/inet.h>

/**
 * @brief Stage2 header size constant
 *
 * Used for validating incoming requests in fast_reply.
 */
#define USBIP_STAGE2_HEADER_SIZE sizeof(usbip_stage2_header)

void usbip_swap_header(void *data, size_t size)
{
    /* Skip the first 8 bytes (setup field) and swap the rest */
    size_t num_words = (size / sizeof(uint32_t)) - 2;
    uint32_t *ptr = (uint32_t *)data;

    for (size_t i = 0; i < num_words; i++)
    {
        ptr[i] = htonl(ptr[i]);
    }
}

void usbip_send_stage2_submit(usbip_context_t *ctx, usbip_stage2_header *req_header,
                               int32_t status, int32_t data_length)
{
    if (ctx == NULL)
    {
        return;
    }

    req_header->base.command = USBIP_STAGE2_RSP_SUBMIT;
    req_header->base.direction = !(req_header->base.direction);

    memset(&(req_header->u.ret_submit), 0, sizeof(usbip_stage2_header_ret_submit));

    req_header->u.ret_submit.status = status;
    req_header->u.ret_submit.data_length = data_length;

    usbip_swap_header(req_header, sizeof(usbip_stage2_header));
    usbip_context_send(ctx, req_header, sizeof(usbip_stage2_header));
}

void usbip_send_stage2_submit_data(usbip_context_t *ctx, usbip_stage2_header *req_header,
                                    int32_t status, const void *data, int32_t data_length)
{
    usbip_send_stage2_submit(ctx, req_header, status, data_length);

    if (data_length && data)
    {
        usbip_context_send(ctx, data, data_length);
    }
}

void usbip_send_stage2_submit_data_fast(usbip_context_t *ctx, usbip_stage2_header *req_header,
                                         const void *data, int32_t data_length)
{
    uint8_t *send_buf = (uint8_t *)req_header;

    if (ctx == NULL)
    {
        return;
    }

    req_header->base.command = htonl(USBIP_STAGE2_RSP_SUBMIT);
    req_header->base.direction = htonl(!(req_header->base.direction));

    memset(&(req_header->u.ret_submit), 0, sizeof(usbip_stage2_header_ret_submit));
    req_header->u.ret_submit.data_length = htonl(data_length);

    /* payload */
    memcpy(&send_buf[sizeof(usbip_stage2_header)], data, data_length);
    usbip_context_send(ctx, send_buf, sizeof(usbip_stage2_header) + data_length);
}

int fast_reply(usbip_context_t *ctx, uint8_t *buf, uint32_t length)
{
    usbip_stage2_header *buf_header = (usbip_stage2_header *)buf;
    uint8_t *data = NULL;
    uint32_t data_len = 0;

    /* Check if this is an EP1 IN request */
    if (ctx && length == USBIP_STAGE2_HEADER_SIZE &&
        buf_header->base.command == htonl(USBIP_STAGE2_REQ_SUBMIT) &&
        buf_header->base.direction == htonl(USBIP_DIR_IN) &&
        buf_header->base.ep == htonl(1))
    {
        /* Response should already be ready from handle_dap_data_request() */
        if (dap_get_response(&data, &data_len))
        {
            usbip_send_stage2_submit_data_fast(ctx, buf_header, data, data_len);
            return 1;
        }
    }

    return 0;
}