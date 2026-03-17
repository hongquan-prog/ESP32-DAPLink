/**
 * @file usbip_service.c
 * @brief USBIP service implementation
 *
 * This module provides the main USBIP service entry point,
 * integrating protocol handling with the transport layer.
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbip/usbip_service.h"
#include "usbip/usbip_context.h"
#include "usbip/usbip_server.h"
#include "dap/DAP_handle.h"
#include "usbip/usbip_config.h"

#include "components/USBIP/usb_handle.h"
#include "components/USBIP/usb_descriptor.h"
#include "components/elaphureLink/elaphureLink_protocol.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>

/* Global context for backward compatibility */
static usbip_context_t *s_current_ctx = NULL;

/* Static buffer for elaphureLink response (>64 bytes, avoid stack allocation) */
static uint8_t s_el_response_buffer[1500];

/**
 * @brief Get current USBIP context (for backward compatibility)
 */
usbip_context_t *usbip_service_get_current_context(void)
{
    return s_current_ctx;
}

/**
 * @brief USBIP attach stage handling
 */
static int _handle_attach(usbip_context_t *ctx, uint8_t *buffer, uint32_t length);

/**
 * @brief USBIP emulate stage handling
 */
static int _handle_emulate(usbip_context_t *ctx, uint8_t *buffer, uint32_t length);

/**
 * @brief Read stage1 command
 */
static int _read_stage1_command(uint8_t *buffer, uint32_t length);

/**
 * @brief Read stage2 command
 */
static int _read_stage2_command(usbip_stage2_header *header, uint32_t length);

/**
 * @brief Pack header for network transmission
 */
static void _pack(void *data, int size);

/**
 * @brief Unpack header from network
 */
static void _unpack(void *data, int size);

/* Stage 1 handlers */
static void _handle_device_list(usbip_context_t *ctx, uint8_t *buffer, uint32_t length);
static void _handle_device_attach(usbip_context_t *ctx, uint8_t *buffer, uint32_t length);
static void _send_stage1_header(usbip_context_t *ctx, uint16_t command, uint32_t status);
static void _send_device_list(usbip_context_t *ctx);
static void _send_device_info(usbip_context_t *ctx);
static void _send_interface_info(usbip_context_t *ctx);

/* Stage 2 handlers */
static int _handle_submit(usbip_context_t *ctx, usbip_stage2_header *header, uint32_t length);
static void _handle_unlink(usbip_context_t *ctx, usbip_stage2_header *header);
static void _send_stage2_unlink(usbip_context_t *ctx, usbip_stage2_header *req_header);

/* USB send operations callbacks for usb_handle.c */
static void _send_submit_data_cb(void *user_data, usbip_stage2_header *req_header,
                                  int32_t status, const void *data, int32_t data_length);
static void _send_submit_cb(void *user_data, usbip_stage2_header *req_header,
                             int32_t status, int32_t data_length);
static void _send_data_cb(void *user_data, const void *data, size_t len);

void usbip_service_task(void *arg)
{
    const transport_ops_t *ops = (const transport_ops_t *)arg;
    usbip_context_t *ctx = NULL;
    int len = 0;

    /* Create USBIP context */
    ctx = usbip_context_create(ops, USBIP_PORT);
    if (ctx == NULL)
    {
        printf("Failed to create USBIP context\r\n");
        vTaskDelete(NULL);
        return;
    }

    /* Initialize transport */
    if (usbip_context_init(ctx) != 0)
    {
        printf("Failed to initialize transport\r\n");
        usbip_context_destroy(ctx);
        vTaskDelete(NULL);
        return;
    }

    printf("USBIP server listening on port %d\r\n", USBIP_PORT);
    s_current_ctx = ctx;

    /* Main service loop */
    while (1)
    {
        /* Wait for connection */
        printf("Waiting for connection...\r\n");
        if (usbip_context_accept(ctx) != 0)
        {
            printf("Accept failed\r\n");
            continue;
        }

        printf("Client connected\r\n");
        ctx->state = USBIP_STATE_ATTACHING;

        /* Set global context for legacy API compatibility */
        usbip_set_global_context(ctx);

        /* Connection loop */
        while (1)
        {
            len = usbip_context_recv(ctx, ctx->rx_buffer, ctx->rx_buffer_size);

            if (len < 0)
            {
                printf("Recv failed: errno %d\r\n", errno);
                break;
            }

            if (len == 0)
            {
                printf("Connection closed\r\n");
                break;
            }

            /* Process data based on state */
            switch (ctx->state)
            {
            case USBIP_STATE_ATTACHING:
                /* Check for elaphureLink handshake */
                if (el_is_handshake_request(ctx->rx_buffer, len))
                {
                    el_response_handshake_t el_res;
                    size_t res_len = el_create_handshake_response(&el_res);
                    usbip_context_send(ctx, &el_res, res_len);

                    ctx->state = USBIP_STATE_EL_DATA_PHASE;
                    dap_set_restart_signal(DAP_SIGNAL_DELETE);
                    el_process_buffer_malloc();
                }
                else
                {
                    _handle_attach(ctx, ctx->rx_buffer, len);
                }
                break;

            case USBIP_STATE_EMULATING:
                _handle_emulate(ctx, ctx->rx_buffer, len);
                break;

            case USBIP_STATE_EL_DATA_PHASE:
                {
                    size_t el_res_len = 0;
                    el_process_dap_command(ctx->rx_buffer, len, s_el_response_buffer, &el_res_len);
                    if (el_res_len > 0)
                    {
                        usbip_context_send(ctx, s_el_response_buffer, el_res_len);
                    }
                }
                break;

            default:
                printf("Unknown state: %d\r\n", ctx->state);
                break;
            }
        }

        /* Connection cleanup */
        printf("Shutting down connection...\r\n");
        usbip_context_close(ctx);

        /* Clear global context */
        usbip_set_global_context(NULL);

        /* Reset DAP */
        el_process_buffer_free();
        dap_set_restart_signal(DAP_SIGNAL_RESET);
        dap_notify_task();

        ctx->state = USBIP_STATE_ACCEPTING;
    }

    /* Cleanup */
    s_current_ctx = NULL;
    usbip_context_destroy(ctx);
    vTaskDelete(NULL);
}

int usbip_service_process_data(usbip_context_t *ctx, uint8_t *buffer, uint32_t length)
{
    if (ctx == NULL)
    {
        return -1;
    }

    switch (ctx->state)
    {
    case USBIP_STATE_ATTACHING:
        return _handle_attach(ctx, buffer, length);

    case USBIP_STATE_EMULATING:
        return _handle_emulate(ctx, buffer, length);

    default:
        return -1;
    }
}

/* Stage 1 implementation */

static int _handle_attach(usbip_context_t *ctx, uint8_t *buffer, uint32_t length)
{
    int command = _read_stage1_command(buffer, length);

    if (command < 0)
    {
        return -1;
    }

    switch (command)
    {
    case USBIP_STAGE1_CMD_DEVICE_LIST:
        _handle_device_list(ctx, buffer, length);
        break;

    case USBIP_STAGE1_CMD_DEVICE_ATTACH:
        _handle_device_attach(ctx, buffer, length);
        break;

    default:
        printf("attach Unknown command: %d\r\n", command);
        break;
    }

    return 0;
}

static int _read_stage1_command(uint8_t *buffer, uint32_t length)
{
    usbip_stage1_header *req = NULL;

    if (length < sizeof(usbip_stage1_header))
    {
        return -1;
    }

    req = (usbip_stage1_header *)buffer;
    return (ntohs(req->command) & 0xFF);
}

static void _handle_device_list(usbip_context_t *ctx, uint8_t *buffer, uint32_t length)
{
    (void)buffer;
    (void)length;

    printf("Handling dev list request...\r\n");
    _send_stage1_header(ctx, USBIP_STAGE1_CMD_DEVICE_LIST, 0);
    _send_device_list(ctx);
}

static void _handle_device_attach(usbip_context_t *ctx, uint8_t *buffer, uint32_t length)
{
    (void)buffer;

    printf("Handling dev attach request...\r\n");

    if (length < sizeof(USBIP_BUSID_SIZE))
    {
        printf("handle device attach failed!\r\n");
        return;
    }

    _send_stage1_header(ctx, USBIP_STAGE1_CMD_DEVICE_ATTACH, 0);
    _send_device_info(ctx);
    ctx->state = USBIP_STATE_EMULATING;
}

static void _send_stage1_header(usbip_context_t *ctx, uint16_t command, uint32_t status)
{
    usbip_stage1_header header;

    printf("Sending header...\r\n");

    header.version = htons(273);
    header.command = htons(command);
    header.status = htonl(status);

    usbip_context_send(ctx, &header, sizeof(usbip_stage1_header));
}

static void _send_device_list(usbip_context_t *ctx)
{
    usbip_stage1_response_devlist response_devlist;

    printf("Sending device list...\r\n");

    /* we have only 1 device */
    response_devlist.list_size = htonl(1);
    usbip_context_send(ctx, &response_devlist, sizeof(usbip_stage1_response_devlist));

    _send_device_info(ctx);
    _send_interface_info(ctx);
}

static void _send_device_info(usbip_context_t *ctx)
{
    usbip_stage1_usb_device device;

    printf("Sending device info...\r\n");

    strcpy(device.path, "/sys/devices/pci0000:00/0000:00:01.2/usb1/1-1");
    strcpy(device.busid, "1-1");

    device.busnum = htonl(1);
    device.devnum = htonl(1);
    device.speed = htonl(3);

    device.idVendor = htons(USBD0_DEV_DESC_IDVENDOR);
    device.idProduct = htons(USBD0_DEV_DESC_IDPRODUCT);
    device.bcdDevice = htons(USBD0_DEV_DESC_BCDDEVICE);

    device.bDeviceClass = 0x00;
    device.bDeviceSubClass = 0x00;
    device.bDeviceProtocol = 0x00;

    device.bConfigurationValue = 1;
    device.bNumConfigurations = 1;
    device.bNumInterfaces = 1;

    usbip_context_send(ctx, &device, sizeof(usbip_stage1_usb_device));
}

static void _send_interface_info(usbip_context_t *ctx)
{
    usbip_stage1_usb_interface interface;

    printf("Sending interface info...\r\n");

    interface.bInterfaceClass = USBD_CUSTOM_CLASS0_IF0_CLASS;
    interface.bInterfaceSubClass = USBD_CUSTOM_CLASS0_IF0_SUBCLASS;
    interface.bInterfaceProtocol = USBD_CUSTOM_CLASS0_IF0_PROTOCOL;
    interface.padding = 0;

    usbip_context_send(ctx, &interface, sizeof(usbip_stage1_usb_interface));
}

/* Stage 2 implementation */

static int _handle_emulate(usbip_context_t *ctx, uint8_t *buffer, uint32_t length)
{
    int command = 0;
    usbip_stage2_header *header = (usbip_stage2_header *)buffer;

    /* Fast reply path for DAP data */
    if (fast_reply_ctx(ctx, buffer, length))
    {
        return 0;
    }

    command = _read_stage2_command(header, length);

    if (command < 0)
    {
        return -1;
    }

    switch (command)
    {
    case USBIP_STAGE2_REQ_SUBMIT:
        return _handle_submit(ctx, header, length);

    case USBIP_STAGE2_REQ_UNLINK:
        _handle_unlink(ctx, header);
        return 0;

    default:
        printf("emulate unknown command:%d\r\n", command);
        return -1;
    }
}

static int _read_stage2_command(usbip_stage2_header *header, uint32_t length)
{
    if (length < sizeof(usbip_stage2_header))
    {
        return -1;
    }

    _unpack((uint32_t *)header, sizeof(usbip_stage2_header));
    return header->base.command;
}

static void _pack(void *data, int size)
{
    int sz = (size / sizeof(uint32_t)) - 2;
    uint32_t *ptr = (uint32_t *)data;

    for (int i = 0; i < sz; i++)
    {
        ptr[i] = htonl(ptr[i]);
    }
}

static void _unpack(void *data, int size)
{
    int sz = (size / sizeof(uint32_t)) - 2;
    uint32_t *ptr = (uint32_t *)data;

    for (int i = 0; i < sz; i++)
    {
        ptr[i] = ntohl(ptr[i]);
    }
}

static void _send_submit_data_cb(void *user_data, usbip_stage2_header *req_header,
                                  int32_t status, const void *data, int32_t data_length)
{
    usbip_context_t *ctx = (usbip_context_t *)user_data;
    usbip_send_stage2_submit_data_ctx(ctx, req_header, status, data, data_length);
}

static void _send_submit_cb(void *user_data, usbip_stage2_header *req_header,
                             int32_t status, int32_t data_length)
{
    usbip_context_t *ctx = (usbip_context_t *)user_data;
    usbip_send_stage2_submit_ctx(ctx, req_header, status, data_length);
}

static void _send_data_cb(void *user_data, const void *data, size_t len)
{
    usbip_context_t *ctx = (usbip_context_t *)user_data;
    usbip_context_send(ctx, data, len);
}

static int _handle_submit(usbip_context_t *ctx, usbip_stage2_header *header, uint32_t length)
{
    (void)length;

    /* Send operations structure for usb_handle.c */
    usb_send_ops_t send_ops = {
        .user_data = ctx,
        .send_submit_data = _send_submit_data_cb,
        .send_submit = _send_submit_cb,
        .send_data = _send_data_cb
    };

    switch (header->base.ep)
    {
    case 0x00:
        handleUSBControlRequest(header, &send_ops);
        break;

    case 0x01:
        if (header->base.direction == 0)
        {
            handle_dap_data_request(header, length);
        }
        else
        {
            handle_dap_data_response(header);
        }
        break;

    case 0x02:
        if (header->base.direction == 0)
        {
            usbip_send_stage2_submit(header, 0, 0);
        }
        else
        {
            handle_swo_trace_response(header);
        }
        break;

    case 0x81:
        printf("*** WARN! EP 81 DATA\r\n");
        return -1;

    default:
        printf("*** WARN! UNKNOWN ENDPOINT: %d\r\n", (int)header->base.ep);
        return -1;
    }

    return 0;
}

static void _handle_unlink(usbip_context_t *ctx, usbip_stage2_header *header)
{
    printf("s2 handling cmd unlink...\r\n");
    handle_dap_unlink();
    _send_stage2_unlink(ctx, header);
}

static void _send_stage2_unlink(usbip_context_t *ctx, usbip_stage2_header *req_header)
{
    req_header->base.command = USBIP_STAGE2_RSP_UNLINK;
    req_header->base.direction = USBIP_DIR_OUT;
    memset(&(req_header->u.ret_unlink), 0, sizeof(usbip_stage2_header_ret_unlink));
    req_header->u.ret_unlink.status = -1;

    _pack(req_header, sizeof(usbip_stage2_header));
    usbip_context_send(ctx, req_header, sizeof(usbip_stage2_header));
}