/*
 * Copyright (c) 2026-2026, hongquan.li
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-3-24      hongquan.li   add license declaration
 */

/*****************************************************************************
 * USBIP Protocol Layer Implementation
 *
 * Implement protocol encoding/decoding and send/receive functions
 *****************************************************************************/

#include "usbip_protocol.h"
#include <endian.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "hal/usbip_transport.h"

/*****************************************************************************
 * Byte Order Conversion Functions
 *****************************************************************************/

void usbip_pack_op_common(struct op_common* op, int to_network)
{
    if (to_network)
    {
        /* Host order to network order */
        op->version = htobe16(op->version);
        op->code = htobe16(op->code);
        op->status = htobe32(op->status);
    }
    else
    {
        /* Network order to host order */
        op->version = be16toh(op->version);
        op->code = be16toh(op->code);
        op->status = be32toh(op->status);
    }
}

void usbip_pack_usb_device(struct usbip_usb_device* udev, int to_network)
{
    if (to_network)
    {
        udev->busnum = htobe32(udev->busnum);
        udev->devnum = htobe32(udev->devnum);
        udev->speed = htobe32(udev->speed);
        udev->idVendor = htobe16(udev->idVendor);
        udev->idProduct = htobe16(udev->idProduct);
        udev->bcdDevice = htobe16(udev->bcdDevice);
    }
    else
    {
        udev->busnum = be32toh(udev->busnum);
        udev->devnum = be32toh(udev->devnum);
        udev->speed = be32toh(udev->speed);
        udev->idVendor = be16toh(udev->idVendor);
        udev->idProduct = be16toh(udev->idProduct);
        udev->bcdDevice = be16toh(udev->bcdDevice);
    }
}

void usbip_pack_usb_interface(struct usbip_usb_interface* uinf, int to_network)
{
    /* Interface descriptors are all uint8_t, no byte order conversion needed */
    (void)uinf;
    (void)to_network;
}

void usbip_pack_header(struct usbip_header* hdr, int to_network)
{
    if (to_network)
    {
        uint32_t cmd = hdr->base.command; /* Save original command value */

        hdr->base.command = htobe32(hdr->base.command);
        hdr->base.seqnum = htobe32(hdr->base.seqnum);
        hdr->base.devid = htobe32(hdr->base.devid);
        hdr->base.direction = htobe32(hdr->base.direction);
        hdr->base.ep = htobe32(hdr->base.ep);

        switch (cmd)
        {
            case USBIP_CMD_SUBMIT:
                hdr->u.cmd_submit.transfer_flags = htobe32(hdr->u.cmd_submit.transfer_flags);
                hdr->u.cmd_submit.transfer_buffer_length =
                    htobe32(hdr->u.cmd_submit.transfer_buffer_length);
                hdr->u.cmd_submit.start_frame = htobe32(hdr->u.cmd_submit.start_frame);
                hdr->u.cmd_submit.number_of_packets = htobe32(hdr->u.cmd_submit.number_of_packets);
                hdr->u.cmd_submit.interval = htobe32(hdr->u.cmd_submit.interval);
                break;
            case USBIP_RET_SUBMIT:
                hdr->u.ret_submit.status = htobe32(hdr->u.ret_submit.status);
                hdr->u.ret_submit.actual_length = htobe32(hdr->u.ret_submit.actual_length);
                hdr->u.ret_submit.start_frame = htobe32(hdr->u.ret_submit.start_frame);
                hdr->u.ret_submit.number_of_packets = htobe32(hdr->u.ret_submit.number_of_packets);
                hdr->u.ret_submit.error_count = htobe32(hdr->u.ret_submit.error_count);
                break;
            case USBIP_CMD_UNLINK:
                hdr->u.cmd_unlink.seqnum = htobe32(hdr->u.cmd_unlink.seqnum);
                break;
            case USBIP_RET_UNLINK:
                hdr->u.ret_unlink.status = htobe32(hdr->u.ret_unlink.status);
                break;
        }
    }
    else
    {
        hdr->base.command = be32toh(hdr->base.command);
        hdr->base.seqnum = be32toh(hdr->base.seqnum);
        hdr->base.devid = be32toh(hdr->base.devid);
        hdr->base.direction = be32toh(hdr->base.direction);
        hdr->base.ep = be32toh(hdr->base.ep);

        switch (hdr->base.command)
        {
            case USBIP_CMD_SUBMIT:
                hdr->u.cmd_submit.transfer_flags = be32toh(hdr->u.cmd_submit.transfer_flags);
                hdr->u.cmd_submit.transfer_buffer_length =
                    be32toh(hdr->u.cmd_submit.transfer_buffer_length);
                hdr->u.cmd_submit.start_frame = be32toh(hdr->u.cmd_submit.start_frame);
                hdr->u.cmd_submit.number_of_packets = be32toh(hdr->u.cmd_submit.number_of_packets);
                hdr->u.cmd_submit.interval = be32toh(hdr->u.cmd_submit.interval);
                break;
            case USBIP_RET_SUBMIT:
                hdr->u.ret_submit.status = be32toh(hdr->u.ret_submit.status);
                hdr->u.ret_submit.actual_length = be32toh(hdr->u.ret_submit.actual_length);
                hdr->u.ret_submit.start_frame = be32toh(hdr->u.ret_submit.start_frame);
                hdr->u.ret_submit.number_of_packets = be32toh(hdr->u.ret_submit.number_of_packets);
                hdr->u.ret_submit.error_count = be32toh(hdr->u.ret_submit.error_count);
                break;
            case USBIP_CMD_UNLINK:
                hdr->u.cmd_unlink.seqnum = be32toh(hdr->u.cmd_unlink.seqnum);
                break;
            case USBIP_RET_UNLINK:
                hdr->u.ret_unlink.status = be32toh(hdr->u.ret_unlink.status);
                break;
        }
    }
}

/*****************************************************************************
 * Protocol Send/Receive Interface
 *****************************************************************************/

int usbip_recv_op_common(struct usbip_conn_ctx* ctx, struct op_common* op)
{
    ssize_t n;

    n = transport_recv(ctx, op, sizeof(*op));
    if (n != sizeof(*op))
    {
        return -1;
    }

    usbip_pack_op_common(op, 0); /* Network order to host order */
    return 0;
}

int usbip_send_op_common(struct usbip_conn_ctx* ctx, uint16_t code, uint32_t status)
{
    struct op_common op = {.version = USBIP_VERSION, .code = code, .status = status};
    ssize_t n;

    usbip_pack_op_common(&op, 1); /* Host order to network order */

    n = transport_send(ctx, &op, sizeof(op));
    if (n != sizeof(op))
    {
        return -1;
    }
    return 0;
}

int usbip_recv_header(struct usbip_conn_ctx* ctx, struct usbip_header* hdr)
{
    ssize_t n;

    n = transport_recv(ctx, hdr, sizeof(*hdr));
    if (n != sizeof(*hdr))
    {
        return -1;
    }

    usbip_pack_header(hdr, 0);
    return 0;
}

int usbip_send_header(struct usbip_conn_ctx* ctx, struct usbip_header* hdr)
{
    ssize_t n;

    /* Convert to network byte order first */
    usbip_pack_header(hdr, 1);
    n = transport_send(ctx, hdr, sizeof(*hdr));
    if (n != sizeof(*hdr))
    {
        return -1;
    }
    return 0;
}