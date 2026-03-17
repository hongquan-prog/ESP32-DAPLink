/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   Initial version
 * 2026-3-17     lihongquan   Refactored according to CODE_STYLE.md
 */

#include "usbip_server.h"

#include "kcp_server.h"
#include "tcp_netconn.h"
#include "DAP_handle.h"
#include "wifi_configuration.h"

#include "components/USBIP/usb_handle.h"
#include "components/USBIP/usb_descriptor.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#include <string.h>

static usbip_state_t s_usbip_state = USBIP_STATE_ACCEPTING;

/* attach helper function */
static int _read_stage1_command(uint8_t *buffer, uint32_t length);
static void _handle_device_list(uint8_t *buffer, uint32_t length);
static void _handle_device_attach(uint8_t *buffer, uint32_t length);
static void _send_stage1_header(uint16_t command, uint32_t status);
static void _send_device_list(void);
static void _send_device_info(void);
static void _send_interface_info(void);

/* emulate helper function */
static void _pack(void *data, int size);
static void _unpack(void *data, int size);
static int _handle_submit(usbip_stage2_header *header, uint32_t length);
static int _read_stage2_command(usbip_stage2_header *header, uint32_t length);
static void _handle_unlink(usbip_stage2_header *header);
static void _send_stage2_unlink(usbip_stage2_header *req_header);

/**
 * @brief Get current USBIP state
 *
 * @return Current state value
 */
usbip_state_t usbip_get_state(void)
{
    return s_usbip_state;
}

/**
 * @brief Set USBIP state
 *
 * @param state State value to set
 */
void usbip_set_state(usbip_state_t state)
{
    s_usbip_state = state;
}

int usbip_network_send(int s, const void *dataptr, size_t size, int flags)
{
#if (USE_KCP == 1)
    return kcp_network_send(dataptr, size);
#elif (USE_TCP_NETCONN == 1)
    return tcp_netconn_send(dataptr, size);
#else
    return send(s, dataptr, size, flags);
#endif
}

int usbip_attach(uint8_t *buffer, uint32_t length)
{
    int command = _read_stage1_command(buffer, length);

    if (command < 0)
    {
        return -1;
    }

    switch (command)
    {
    case USBIP_STAGE1_CMD_DEVICE_LIST:
        _handle_device_list(buffer, length);
        break;

    case USBIP_STAGE1_CMD_DEVICE_ATTACH:
        _handle_device_attach(buffer, length);
        break;

    default:
        os_printf("attach Unknown command: %d\r\n", command);
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

static void _handle_device_list(uint8_t *buffer, uint32_t length)
{
    os_printf("Handling dev list request...\r\n");
    _send_stage1_header(USBIP_STAGE1_CMD_DEVICE_LIST, 0);
    _send_device_list();
}

static void _handle_device_attach(uint8_t *buffer, uint32_t length)
{
    os_printf("Handling dev attach request...\r\n");

    if (length < sizeof(USBIP_BUSID_SIZE))
    {
        os_printf("handle device attach failed!\r\n");
        return;
    }

    _send_stage1_header(USBIP_STAGE1_CMD_DEVICE_ATTACH, 0);
    _send_device_info();
    usbip_set_state(USBIP_STATE_EMULATING);
}

static void _send_stage1_header(uint16_t command, uint32_t status)
{
    usbip_stage1_header header;

    os_printf("Sending header...\r\n");

    header.version = htons(273);
    header.command = htons(command);
    header.status = htonl(status);

    usbip_network_send(kSock, (uint8_t *)&header, sizeof(usbip_stage1_header), 0);
}

static void _send_device_list(void)
{
    usbip_stage1_response_devlist response_devlist;

    os_printf("Sending device list...\r\n");
    os_printf("Sending device list size...\r\n");

    /* we have only 1 device */
    response_devlist.list_size = htonl(1);
    usbip_network_send(kSock, (uint8_t *)&response_devlist, sizeof(usbip_stage1_response_devlist), 0);

    /* send device info */
    _send_device_info();

    /* send device interfaces */
    _send_interface_info();
}

static void _send_device_info(void)
{
    usbip_stage1_usb_device device;

    os_printf("Sending device info...\r\n");

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

    usbip_network_send(kSock, (uint8_t *)&device, sizeof(usbip_stage1_usb_device), 0);
}

static void _send_interface_info(void)
{
    usbip_stage1_usb_interface interface;

    os_printf("Sending interface info...\r\n");

    interface.bInterfaceClass = USBD_CUSTOM_CLASS0_IF0_CLASS;
    interface.bInterfaceSubClass = USBD_CUSTOM_CLASS0_IF0_SUBCLASS;
    interface.bInterfaceProtocol = USBD_CUSTOM_CLASS0_IF0_PROTOCOL;
    interface.padding = 0;

    usbip_network_send(kSock, (uint8_t *)&interface, sizeof(usbip_stage1_usb_interface), 0);
}

int usbip_emulate(uint8_t *buffer, uint32_t length)
{
    int command = 0;

    if (fast_reply(buffer, length))
    {
        return 0;
    }

    command = _read_stage2_command((usbip_stage2_header *)buffer, length);

    if (command < 0)
    {
        return -1;
    }

    switch (command)
    {
    case USBIP_STAGE2_REQ_SUBMIT:
        _handle_submit((usbip_stage2_header *)buffer, length);
        break;

    case USBIP_STAGE2_REQ_UNLINK:
        _handle_unlink((usbip_stage2_header *)buffer);
        break;

    default:
        os_printf("emulate unknown command:%d\r\n", command);
        return -1;
    }

    return 0;
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

/**
 * @brief Pack the following packets(Offset 0x00 - 0x28):
 *        - cmd_submit
 *        - ret_submit
 *        - cmd_unlink
 *        - ret_unlink
 *
 * @param data Point to packets header
 * @param size Packets header size
 */
static void _pack(void *data, int size)
{
    int sz = (size / sizeof(uint32_t)) - 2;
    uint32_t *ptr = (uint32_t *)data;

    /* ignore the setup field */
    for (int i = 0; i < sz; i++)
    {
        ptr[i] = htonl(ptr[i]);
    }
}

/**
 * @brief Unpack the following packets(Offset 0x00 - 0x28):
 *        - cmd_submit
 *        - ret_submit
 *        - cmd_unlink
 *        - ret_unlink
 *
 * @param data Point to packets header
 * @param size Packets header size
 */
static void _unpack(void *data, int size)
{
    int sz = (size / sizeof(uint32_t)) - 2;
    uint32_t *ptr = (uint32_t *)data;

    /* ignore the setup field */
    for (int i = 0; i < sz; i++)
    {
        ptr[i] = ntohl(ptr[i]);
    }
}

/**
 * @brief USB transaction processing
 *
 * @param header USBIP stage2 header
 * @param length Data length
 * @return 0 on success, negative on error
 */
static int _handle_submit(usbip_stage2_header *header, uint32_t length)
{
    switch (header->base.ep)
    {
    /* control endpoint(endpoint 0) */
    case 0x00:
        handleUSBControlRequest(header);
        break;

    /* endpoint 1 data receive and response */
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

    /* endpoint 2 for SWO trace */
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

    /* request to save data to device */
    case 0x81:
        if (header->base.direction == 0)
        {
            os_printf("*** WARN! EP 81 DATA TX");
        }
        else
        {
            os_printf("*** WARN! EP 81 DATA RX");
        }
        return -1;

    default:
        os_printf("*** WARN ! UNKNOWN ENDPOINT: %d\r\n", (int)header->base.ep);
        return -1;
    }

    return 0;
}

void usbip_send_stage2_submit(usbip_stage2_header *req_header, int32_t status, int32_t data_length)
{
    req_header->base.command = USBIP_STAGE2_RSP_SUBMIT;
    req_header->base.direction = !(req_header->base.direction);

    memset(&(req_header->u.ret_submit), 0, sizeof(usbip_stage2_header_ret_submit));

    req_header->u.ret_submit.status = status;
    req_header->u.ret_submit.data_length = data_length;

    _pack(req_header, sizeof(usbip_stage2_header));
    usbip_network_send(kSock, req_header, sizeof(usbip_stage2_header), 0);
}

void usbip_send_stage2_submit_data(usbip_stage2_header *req_header, int32_t status, const void *const data, int32_t data_length)
{
    usbip_send_stage2_submit(req_header, status, data_length);

    if (data_length)
    {
        usbip_network_send(kSock, data, data_length, 0);
    }
}

void usbip_send_stage2_submit_data_fast(usbip_stage2_header *req_header, const void *const data, int32_t data_length)
{
    uint8_t *send_buf = (uint8_t *)req_header;

    req_header->base.command = PP_HTONL(USBIP_STAGE2_RSP_SUBMIT);
    req_header->base.direction = htonl(!(req_header->base.direction));

    memset(&(req_header->u.ret_submit), 0, sizeof(usbip_stage2_header_ret_submit));
    req_header->u.ret_submit.data_length = htonl(data_length);

    /* payload */
    memcpy(&send_buf[sizeof(usbip_stage2_header)], data, data_length);
    usbip_network_send(kSock, send_buf, sizeof(usbip_stage2_header) + data_length, 0);
}

static void _handle_unlink(usbip_stage2_header *header)
{
    os_printf("s2 handling cmd unlink...\r\n");
    handle_dap_unlink();
    _send_stage2_unlink(header);
}

static void _send_stage2_unlink(usbip_stage2_header *req_header)
{
    req_header->base.command = USBIP_STAGE2_RSP_UNLINK;
    req_header->base.direction = USBIP_DIR_OUT;
    memset(&(req_header->u.ret_unlink), 0, sizeof(usbip_stage2_header_ret_unlink));

    /*
     * To be more precise, the value is `-ECONNRESET`, but usbip-win only cares if it is a
     * non zero value. A non-zero value indicates that our UNLINK operation was "successful",
     * but the host driver's may behave differently, or may even ignore this state. For consistent
     * behavior, we use non-zero value here. See also comments regarding `handle_dap_unlink()`.
     */
    req_header->u.ret_unlink.status = -1;

    _pack(req_header, sizeof(usbip_stage2_header));
    usbip_network_send(kSock, req_header, sizeof(usbip_stage2_header), 0);
}