/*
 * Copyright (c) 2026-2026, hongquan.li
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-3-24      hongquan.li   add license declaration
 */

/*
 * Virtual Custom Bulk Device (Refactored)
 *
 * Custom Bulk device for testing Bulk IN/OUT transfers
 * Uses usbip_common.h framework
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "hal/usbip_log.h"
#include "hal/usbip_osal.h"
#include "usbip_common.h"
#include "usbip_devmgr.h"

LOG_MODULE_REGISTER(bulk, CONFIG_USBIP_LOG_LEVEL);

/*****************************************************************************
 * Configuration
 *****************************************************************************/

#define BULK_MAX_PACKET 512
#define BULK_BUFFER_SIZE 256 /* Satisfy 10KB limit */

/* Custom commands */
#define CMD_ECHO 0x01
#define CMD_GET_INFO 0x02
#define CMD_SET_LED 0x03
#define CMD_GET_COUNTER 0x04

/*****************************************************************************
 * Device Descriptor
 *****************************************************************************/

static const struct usb_device_descriptor bulk_dev_desc = {
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0xFF, /* Vendor Specific */
    .bDeviceSubClass = 0xFF,
    .bDeviceProtocol = 0xFF,
    .bMaxPacketSize0 = 64,
    .idVendor = 0x1234,
    .idProduct = 0x0003,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

/*****************************************************************************
 * Configuration Descriptor
 *****************************************************************************/

struct bulk_config_desc
{
    struct usb_config_descriptor config;
    struct usb_interface_descriptor intf;
    struct usb_endpoint_descriptor ep_in;
    struct usb_endpoint_descriptor ep_out;
} __attribute__((packed));

static const struct bulk_config_desc bulk_cfg_desc = {
    .config =
        {
            .bLength = USB_DT_CONFIG_SIZE,
            .bDescriptorType = USB_DT_CONFIG,
            .wTotalLength = sizeof(struct bulk_config_desc),
            .bNumInterfaces = 1,
            .bConfigurationValue = 1,
            .iConfiguration = 0,
            .bmAttributes = 0x80,
            .bMaxPower = 50,
        },
    .intf =
        {
            .bLength = USB_DT_INTERFACE_SIZE,
            .bDescriptorType = USB_DT_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 2,
            .bInterfaceClass = USB_CLASS_VENDOR_SPEC,
            .bInterfaceSubClass = 0xFF,
            .bInterfaceProtocol = 0xFF,
            .iInterface = 0,
        },
    .ep_in =
        {
            .bLength = USB_DT_ENDPOINT_SIZE,
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = 0x81, /* EP1 IN */
            .bmAttributes = USB_ENDPOINT_XFER_BULK,
            .wMaxPacketSize = BULK_MAX_PACKET,
            .bInterval = 0,
        },
    .ep_out =
        {
            .bLength = USB_DT_ENDPOINT_SIZE,
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = 0x02, /* EP2 OUT */
            .bmAttributes = USB_ENDPOINT_XFER_BULK,
            .wMaxPacketSize = BULK_MAX_PACKET,
            .bInterval = 0,
        },
};

/*****************************************************************************
 * String Descriptors
 *****************************************************************************/

static const uint8_t string0_desc[] = {0x04, USB_DT_STRING, 0x09, 0x04};
static const uint8_t string1_desc[] = {
    0x1C, USB_DT_STRING, 'R', 0,   'a', 0,   's', 0,   'p', 0,   'b', 0,   'e',
    0,    'r',           0,   'r', 0,   'y', 0,   ' ', 0,   'P', 0,   'i', 0};
static const uint8_t string2_desc[] = {
    0x1A, USB_DT_STRING, 'B', 0,   'u', 0,   'l', 0,   'k', 0,   ' ', 0, 'D',
    0,    'e',           0,   'v', 0,   'i', 0,   'c', 0,   'e', 0};

static const uint8_t* bulk_string_descs[] = {string1_desc, string2_desc, string0_desc};

/*****************************************************************************
 * Virtual Bulk Device State
 *****************************************************************************/

struct virtual_bulk
{
    struct usbip_usb_device udev;
    int exported;
    struct usbip_conn_ctx* ctx;
    uint8_t config_value;
    uint8_t buffer[BULK_BUFFER_SIZE];
    uint32_t counter;
};

static struct virtual_bulk vbulk;

/*****************************************************************************
 * Static Function Declarations
 *****************************************************************************/

/**
 * vbulk_handle_urb - Handle URB request
 * @driver: Device driver pointer
 * @urb_cmd: URB command
 * @urb_ret: URB return
 * @data_out: Output data pointer
 * @data_len: Output data length
 * @urb_data: URB data
 * @urb_data_len: URB data length
 * Return: 1 on success, -1 on error
 */
static int vbulk_handle_urb(struct usbip_device_driver* driver, const struct usbip_header* urb_cmd,
                            struct usbip_header* urb_ret, void** data_out, size_t* data_len,
                            const void* urb_data, size_t urb_data_len);

/**
 * vbulk_get_device_count - Get number of devices
 * @driver: Device driver pointer
 * Return: Number of devices
 */
static int vbulk_get_device_count(struct usbip_device_driver* driver);

/**
 * vbulk_get_device_by_index - Get device by index
 * @driver: Device driver pointer
 * @index: Device index
 * @device: Output device info
 * Return: 0 on success, -1 on error
 */
static int vbulk_get_device_by_index(struct usbip_device_driver* driver, int index,
                                     struct usbip_usb_device* device);

/**
 * vbulk_get_device - Get device info by bus ID
 * @driver: Device driver pointer
 * @busid: Bus ID
 * Return: Device pointer, NULL if not found
 */
static const struct usbip_usb_device* vbulk_get_device(struct usbip_device_driver* driver,
                                                       const char* busid);

/**
 * vbulk_export_device - Export device
 * @driver: Device driver pointer
 * @busid: Bus ID
 * @ctx: Connection context
 * Return: 0 on success, -1 on error
 */
static int vbulk_export_device(struct usbip_device_driver* driver, const char* busid,
                               struct usbip_conn_ctx* ctx);

/**
 * vbulk_unexport_device - Unexport device
 * @driver: Device driver pointer
 * @busid: Bus ID
 * Return: 0 on success, -1 on error
 */
static int vbulk_unexport_device(struct usbip_device_driver* driver, const char* busid);

/**
 * vbulk_init - Initialize driver
 * @driver: Device driver pointer
 * Return: 0 on success, -1 on error
 */
static int vbulk_init(struct usbip_device_driver* driver);

/**
 * vbulk_cleanup - Cleanup driver resources
 * @driver: Device driver pointer
 */
static void vbulk_cleanup(struct usbip_device_driver* driver);

/*****************************************************************************
 * URB Handling
 *****************************************************************************/

static int vbulk_handle_urb(struct usbip_device_driver* driver, const struct usbip_header* urb_cmd,
                            struct usbip_header* urb_ret, void** data_out, size_t* data_len,
                            const void* urb_data, size_t urb_data_len)
{
    (void)driver;
    (void)urb_data;

    memset(urb_ret, 0, sizeof(*urb_ret));
    urb_ret->base.command = USBIP_RET_SUBMIT;
    urb_ret->base.seqnum = urb_cmd->base.seqnum;
    urb_ret->base.devid = urb_cmd->base.devid;
    urb_ret->base.direction = urb_cmd->base.direction;
    urb_ret->base.ep = urb_cmd->base.ep;

    if (urb_cmd->base.command == USBIP_CMD_UNLINK)
    {
        urb_ret->base.command = USBIP_RET_UNLINK;
        urb_ret->u.ret_unlink.status = 0;
        return 1;
    }

    if (urb_cmd->base.command != USBIP_CMD_SUBMIT)
    {
        return -1;
    }

    uint32_t ep = urb_cmd->base.ep;

    if (ep == 0)
    {
        /* Control transfer */
        const struct usb_setup_packet* setup =
            (const struct usb_setup_packet*)urb_cmd->u.cmd_submit.setup;

        switch (setup->bRequest)
        {
            case USB_REQ_GET_DESCRIPTOR:
                switch (setup->wValue >> 8)
                {
                    case USB_DT_DEVICE:
                        *data_out = osal_malloc(USB_DT_DEVICE_SIZE);
                        if (*data_out)
                        {
                            memcpy(*data_out, &bulk_dev_desc, USB_DT_DEVICE_SIZE);
                            *data_len = USB_DT_DEVICE_SIZE;
                        }
                        break;
                    case USB_DT_CONFIG:
                        *data_out = osal_malloc(sizeof(bulk_cfg_desc));
                        if (*data_out)
                        {
                            memcpy(*data_out, &bulk_cfg_desc, sizeof(bulk_cfg_desc));
                            *data_len = sizeof(bulk_cfg_desc);
                        }
                        break;
                    case USB_DT_STRING: {
                        uint8_t idx = setup->wValue & 0xFF;
                        if (idx == 0)
                        {
                            *data_out = osal_malloc(sizeof(string0_desc));
                            if (*data_out)
                            {
                                memcpy(*data_out, string0_desc, sizeof(string0_desc));
                                *data_len = sizeof(string0_desc);
                            }
                        }
                        else if (idx <= 3)
                        {
                            const uint8_t* s = bulk_string_descs[idx - 1];
                            *data_out = osal_malloc(s[0]);
                            if (*data_out)
                            {
                                memcpy(*data_out, s, s[0]);
                                *data_len = s[0];
                            }
                        }
                    }
                    break;
                }
                urb_ret->u.ret_submit.actual_length = *data_len;
                urb_ret->u.ret_submit.status = 0;
                break;

            case USB_REQ_SET_CONFIGURATION:
                vbulk.config_value = setup->wValue & 0xFF;
                urb_ret->u.ret_submit.actual_length = 0;
                urb_ret->u.ret_submit.status = 0;
                break;

            default:
                urb_ret->u.ret_submit.actual_length = 0;
                urb_ret->u.ret_submit.status = 0;
                break;
        }
    }
    else if (ep == 1 && urb_cmd->base.direction == USBIP_DIR_IN)
    {
        /* Bulk IN */
        *data_out = osal_malloc(BULK_BUFFER_SIZE);
        if (*data_out)
        {
            memcpy(*data_out, vbulk.buffer, BULK_BUFFER_SIZE);
            *data_len = BULK_BUFFER_SIZE;
        }
        urb_ret->u.ret_submit.actual_length = BULK_BUFFER_SIZE;
        urb_ret->u.ret_submit.status = 0;
    }
    else if (ep == 2 && urb_cmd->base.direction == USBIP_DIR_OUT)
    {
        /* Bulk OUT - handle command */
        if (urb_data && urb_data_len > 0)
        {
            const uint8_t* cmd = urb_data;
            switch (cmd[0])
            {
                case CMD_ECHO:
                    memcpy(vbulk.buffer, urb_data, urb_data_len);
                    break;
                case CMD_GET_COUNTER:
                    vbulk.counter++;
                    memcpy(vbulk.buffer, &vbulk.counter, sizeof(vbulk.counter));
                    break;
            }
        }
        urb_ret->u.ret_submit.actual_length = urb_data_len;
        urb_ret->u.ret_submit.status = 0;
    }
    else
    {
        urb_ret->u.ret_submit.actual_length = 0;
        urb_ret->u.ret_submit.status = 0;
    }

    return 1;
}

/*****************************************************************************
 * Device Enumeration Interface
 *****************************************************************************/

static int vbulk_get_device_count(struct usbip_device_driver* driver)
{
    (void)driver;

    if (vbulk.udev.busid[0] == '\0' || usbip_is_device_busy(vbulk.udev.busid))
    {
        return 0;
    }

    return 1;
}

static int vbulk_get_device_by_index(struct usbip_device_driver* driver, int index,
                                     struct usbip_usb_device* device)
{
    (void)driver;

    if (index != 0 || vbulk.udev.busid[0] == '\0' || usbip_is_device_busy(vbulk.udev.busid))
    {
        return -1;
    }

    memcpy(device, &vbulk.udev, sizeof(*device));

    return 0;
}

static const struct usbip_usb_device* vbulk_get_device(struct usbip_device_driver* driver,
                                                       const char* busid)
{
    (void)driver;
    if (strcmp(vbulk.udev.busid, busid) == 0)
    {
        return &vbulk.udev;
    }
    return NULL;
}

static int vbulk_export_device(struct usbip_device_driver* driver, const char* busid,
                               struct usbip_conn_ctx* ctx)
{
    (void)driver;
    if (strcmp(vbulk.udev.busid, busid) != 0 || vbulk.exported)
    {
        return -1;
    }

    vbulk.exported = 1;
    vbulk.ctx = ctx;
    usbip_set_device_busy(busid);
    LOG_INF("Device exported: %s", busid);

    return 0;
}

static int vbulk_unexport_device(struct usbip_device_driver* driver, const char* busid)
{
    (void)driver;
    if (strcmp(vbulk.udev.busid, busid) != 0)
    {
        return -1;
    }

    vbulk.exported = 0;
    vbulk.ctx = NULL;
    usbip_set_device_available(busid);
    LOG_INF("Device unexported: %s", busid);

    return 0;
}

/*****************************************************************************
 * Initialization and Cleanup
 *****************************************************************************/

static int vbulk_init(struct usbip_device_driver* driver)
{
    (void)driver;

    memset(&vbulk, 0, sizeof(vbulk));
    strncpy(vbulk.udev.path, "/devices/pci0000:00/0000:00:01.0/usb1/1-4", SYSFS_PATH_MAX - 1);
    strncpy(vbulk.udev.busid, "1-4", SYSFS_BUS_ID_SIZE - 1);
    vbulk.udev.busnum = 1;
    vbulk.udev.devnum = 4;
    vbulk.udev.speed = USB_SPEED_FULL;
    vbulk.udev.idVendor = bulk_dev_desc.idVendor;
    vbulk.udev.idProduct = bulk_dev_desc.idProduct;
    vbulk.udev.bcdDevice = bulk_dev_desc.bcdDevice;
    vbulk.udev.bDeviceClass = bulk_dev_desc.bDeviceClass;
    vbulk.udev.bDeviceSubClass = bulk_dev_desc.bDeviceSubClass;
    vbulk.udev.bDeviceProtocol = bulk_dev_desc.bDeviceProtocol;
    vbulk.udev.bConfigurationValue = 1;
    vbulk.udev.bNumConfigurations = bulk_dev_desc.bNumConfigurations;
    vbulk.udev.bNumInterfaces = 1;
    vbulk.exported = 0;
    vbulk.ctx = NULL;

    LOG_INF("Bulk: Initialized custom bulk device");
    return 0;
}

static void vbulk_cleanup(struct usbip_device_driver* driver)
{
    (void)driver;
    memset(&vbulk, 0, sizeof(vbulk));
}

/*****************************************************************************
 * Driver Definition
 *****************************************************************************/

struct usbip_device_driver virtual_bulk_driver = {
    .name = "virtual-bulk",
    .get_device_count = vbulk_get_device_count,
    .get_device_by_index = vbulk_get_device_by_index,
    .get_device = vbulk_get_device,
    .export_device = vbulk_export_device,
    .unexport_device = vbulk_unexport_device,
    .handle_urb = vbulk_handle_urb,
    .init = vbulk_init,
    .cleanup = vbulk_cleanup,
};