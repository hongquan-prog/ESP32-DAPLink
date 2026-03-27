/*
 * Copyright (c) 2026-2026, hongquan.li
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-3-24      hongquan.li   add license declaration
 */

#ifndef USBIP_PROTOCOL_H
#define USBIP_PROTOCOL_H

#include <stdint.h>
#include "hal/usbip_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * USBIP Protocol Constants
 *****************************************************************************/

#define USBIP_VERSION 0x0111

/* Operation code */
#define OP_REQUEST (0x80 << 8)
#define OP_REPLY (0x00 << 8)

#define OP_REQ_DEVLIST (OP_REQUEST | 0x05)
#define OP_REP_DEVLIST (OP_REPLY | 0x05)
#define OP_REQ_IMPORT (OP_REQUEST | 0x03)
#define OP_REP_IMPORT (OP_REPLY | 0x03)

/* URB command */
#define USBIP_CMD_SUBMIT 0x0001
#define USBIP_CMD_UNLINK 0x0002
#define USBIP_RET_SUBMIT 0x0003
#define USBIP_RET_UNLINK 0x0004

/* Direction */
#define USBIP_DIR_OUT 0x00
#define USBIP_DIR_IN 0x01

/* Status code */
#define ST_OK 0x00
#define ST_NA 0x01
#define ST_DEV_BUSY 0x02
#define ST_DEV_ERR 0x03
#define ST_NODEV 0x04
#define ST_ERROR 0x05

/* USB speed */
#define USB_SPEED_UNKNOWN 0
#define USB_SPEED_LOW 1
#define USB_SPEED_FULL 2
#define USB_SPEED_HIGH 3
#define USB_SPEED_WIRELESS 4
#define USB_SPEED_SUPER 5
#define USB_SPEED_SUPER_PLUS 6

/*****************************************************************************
 * Protocol Data Structures (compatible with official implementation)
 *****************************************************************************/

#define SYSFS_PATH_MAX 256
#define SYSFS_BUS_ID_SIZE 32

/* Operation common header (8 bytes) */
struct op_common
{
    uint16_t version;
    uint16_t code;
    uint32_t status;
} __attribute__((packed));

/* USB device descriptor (312 bytes) */
struct usbip_usb_device
{
    char path[SYSFS_PATH_MAX];
    char busid[SYSFS_BUS_ID_SIZE];
    uint32_t busnum;
    uint32_t devnum;
    uint32_t speed;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bConfigurationValue;
    uint8_t bNumConfigurations;
    uint8_t bNumInterfaces;
} __attribute__((packed));

/* USB interface descriptor (4 bytes) */
struct usbip_usb_interface
{
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t padding;
} __attribute__((packed));

/* URB Base Header (20 bytes) */
struct usbip_header_basic
{
    uint32_t command;
    uint32_t seqnum;
    uint32_t devid;
    uint32_t direction;
    uint32_t ep;
} __attribute__((packed));

/* SUBMIT Command (28 bytes) */
struct usbip_header_cmd_submit
{
    uint32_t transfer_flags;
    int32_t transfer_buffer_length;
    int32_t start_frame;
    int32_t number_of_packets;
    int32_t interval;
    uint8_t setup[8];
} __attribute__((packed));

/* RET_SUBMIT (20 bytes) */
struct usbip_header_ret_submit
{
    int32_t status;
    int32_t actual_length;
    int32_t start_frame;
    int32_t number_of_packets;
    int32_t error_count;
} __attribute__((packed));

/* UNLINK Command (4 bytes) */
struct usbip_header_cmd_unlink
{
    uint32_t seqnum;
} __attribute__((packed));

/* RET_UNLINK (4 bytes) */
struct usbip_header_ret_unlink
{
    int32_t status;
} __attribute__((packed));

/* URB Full Header (48 bytes) */
struct usbip_header
{
    struct usbip_header_basic base;
    union
    {
        struct usbip_header_cmd_submit cmd_submit;
        struct usbip_header_ret_submit ret_submit;
        struct usbip_header_cmd_unlink cmd_unlink;
        struct usbip_header_ret_unlink ret_unlink;
    } u;
} __attribute__((packed));

/*****************************************************************************
 * USB Descriptor Constants
 *****************************************************************************/

#define USB_DT_DEVICE 0x01
#define USB_DT_CONFIG 0x02
#define USB_DT_STRING 0x03
#define USB_DT_INTERFACE 0x04
#define USB_DT_ENDPOINT 0x05
#define USB_DT_DEVICE_QUALIFIER 0x06
#define USB_DT_OTHER_SPEED_CONFIG 0x07
#define USB_DT_HID 0x21
#define USB_DT_REPORT 0x22

/* USB request type */
#define USB_REQ_GET_STATUS 0x00
#define USB_REQ_CLEAR_FEATURE 0x01
#define USB_REQ_SET_FEATURE 0x03
#define USB_REQ_SET_ADDRESS 0x05
#define USB_REQ_GET_DESCRIPTOR 0x06
#define USB_REQ_SET_DESCRIPTOR 0x07
#define USB_REQ_GET_CONFIGURATION 0x08
#define USB_REQ_SET_CONFIGURATION 0x09
#define USB_REQ_GET_INTERFACE 0x0A
#define USB_REQ_SET_INTERFACE 0x0B

/* Endpoint Direction */
#define USB_DIR_OUT 0x00
#define USB_DIR_IN 0x80

/* Endpoint Type */
#define USB_ENDPOINT_XFER_CONTROL 0x00
#define USB_ENDPOINT_XFER_ISOC 0x01
#define USB_ENDPOINT_XFER_BULK 0x02
#define USB_ENDPOINT_XFER_INT 0x03

/* Standard Request Type bmRequestType */
#define USB_TYPE_STANDARD (0x00 << 5)
#define USB_TYPE_CLASS (0x01 << 5)
#define USB_TYPE_VENDOR (0x02 << 5)

#define USB_RECIP_DEVICE 0x00
#define USB_RECIP_INTERFACE 0x01
#define USB_RECIP_ENDPOINT 0x02

/*****************************************************************************
 * Protocol Encode/Decode Functions
 *****************************************************************************/

/**
 * usbip_pack_op_common - Convert op_common byte order
 * @op: Operation header pointer
 * @to_network: 1=host to network, 0=network to host
 */
void usbip_pack_op_common(struct op_common* op, int to_network);

/**
 * usbip_pack_usb_device - Convert usbip_usb_device byte order
 * @udev: Device descriptor pointer
 * @to_network: 1=host to network, 0=network to host
 */
void usbip_pack_usb_device(struct usbip_usb_device* udev, int to_network);

/**
 * usbip_pack_usb_interface - Convert usbip_usb_interface byte order
 * @uinf: Interface descriptor pointer
 * @to_network: 1=host to network, 0=network to host
 */
void usbip_pack_usb_interface(struct usbip_usb_interface* uinf, int to_network);

/**
 * usbip_pack_header - Convert usbip_header byte order
 * @hdr: URB header pointer
 * @to_network: 1=host to network, 0=network to host
 */
void usbip_pack_header(struct usbip_header* hdr, int to_network);

/*****************************************************************************
 * Protocol Send/Receive Functions
 *****************************************************************************/

/**
 * usbip_recv_op_common - Receive operation header
 * @ctx: Connection context
 * @op: Output operation header
 * Return: 0 on success, negative on failure
 */
int usbip_recv_op_common(struct usbip_conn_ctx* ctx, struct op_common* op);

/**
 * usbip_send_op_common - Send operation header
 * @ctx: Connection context
 * @code: Operation code
 * @status: Status code
 * Return: 0 on success, negative on failure
 */
int usbip_send_op_common(struct usbip_conn_ctx* ctx, uint16_t code, uint32_t status);

/**
 * usbip_recv_header - Receive URB header
 * @ctx: Connection context
 * @hdr: Output URB header
 * Return: 0 on success, negative on failure
 */
int usbip_recv_header(struct usbip_conn_ctx* ctx, struct usbip_header* hdr);

/**
 * usbip_send_header - Send URB header
 * @ctx: Connection context
 * @hdr: URB header
 * Return: 0 on success, negative on failure
 */
int usbip_send_header(struct usbip_conn_ctx* ctx, struct usbip_header* hdr);

#ifdef __cplusplus
}
#endif

#endif /* USBIP_PROTOCOL_H */