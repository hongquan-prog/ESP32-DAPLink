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
 * Virtual CMSIS-DAP HID Device
 *
 * Virtual CMSIS-DAP debug probe via USBIP
 * Uses Raspberry Pi GPIO for SWD/JTAG debugging
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "DAP.h"
#include "DAP_config.h"
#include "hal/usbip_log.h"
#include "hal/usbip_osal.h"
#include "usbip_common.h"
#include "usbip_control.h"
#include "usbip_devmgr.h"
#include "usbip_hid.h"

LOG_MODULE_REGISTER(dap, CONFIG_DAP_LOG_LEVEL);

/**************************************************************************
 * DAP Device Configuration
 **************************************************************************/

#define DAP_VID 0xFAED
#define DAP_PID 0x4873
#define HID_DAP_PACKET_SIZE 64

/**************************************************************************
 * BOS Descriptor - Contains Microsoft OS 2.0 Platform Capability
 **************************************************************************/

#define MS_OS_20_VENDOR_CODE 0x01
#define MS_OS_20_SET_LEN 0xA2

static const uint8_t dap_bos_desc[] = {
    /* BOS Descriptor Header */
    0x05,       /* bLength */
    0x0F,       /* bDescriptorType: BOS */
    0x21, 0x00, /* wTotalLength: 33 bytes */
    0x01,       /* bNumDeviceCaps: 1 */

    /* Microsoft OS 2.0 Platform Capability Descriptor */
    0x1C, /* bLength: 28 bytes */
    0x10, /* bDescriptorType: Device Capability */
    0x05, /* bDevCapabilityType: Platform */
    0x00, /* bReserved */
    /* PlatformCapabilityUUID: d8dd60df-4589-4cc7-9cd2-659d9e648a9f */
    0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C, 0x9C, 0xD2, 0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F,
    /* Microsoft OS 2.0 specific fields */
    0x00, 0x00, 0x03, 0x06, /* dwWindowsVersion: Windows 8.1 */
    MS_OS_20_SET_LEN, 0x00, /* wMSOSDescriptorSetTotalLength */
    MS_OS_20_VENDOR_CODE,   /* bMS_VendorCode */
    0x00,                   /* bAltEnumCode */
};

/* clang-format off */
static const uint8_t dap_msos20_desc[] = {
    // Microsoft OS 2.0 Descriptor Set header (Table 10)
    0x0A, 0x00,  // wLength
    0x00, 0x00,  // wDescriptorType (Set Header)
    0x00, 0x00, 0x03, 0x06,  // dwWindowsVersion: Windows 8.1
    MS_OS_20_SET_LEN, 0x00,  // wTotalLength

    // Microsoft OS 2.0 compatible ID descriptor (Table 13)
    0x14, 0x00,  // wLength
    0x03, 0x00,  // wDescriptorType (Compatible ID)
    'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,  // compatibleID
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // subCompatibleID

    // Microsoft OS 2.0 registry property descriptor (Table 14)
    0x84, 0x00,  // wLength
    0x04, 0x00,  // wDescriptorType (Registry Property)
    0x07, 0x00,  // wPropertyDataType: REG_MULTI_SZ
    0x2A, 0x00,  // wPropertyNameLength (42 bytes)
    // PropertyName: DeviceInterfaceGUIDs
    'D',0,'e',0,'v',0,'i',0,'c',0,'e',0,'I',0,'n',0,'t',0,'e',0,'r',0,'f',0,'a',0,'c',0,'e',0,'G',0,'U',0,'I',0,'D',0,'s',0, 0,0,
    0x50, 0x00,  // wPropertyDataLength (80 bytes)
    // PropertyData: CMSIS-DAP V2 GUID
    '{',0,'C',0,'D',0,'B',0,'3',0,'B',0,'5',0,'A',0,'D',0,'-',0,'2',0,'9',0,'3',0,'B',0,'-',0,'4',0,'6',0,'6',0,'3',0,'-',0,'A',0,'A',0,'3',0,'6',0,'-',0,'1',0,'A',0,'A',0,'E',0,'4',0,'6',0,'4',0,'6',0,'3',0,'7',0,'7',0,'6',0,'}',0, 0,0, 0,0,
};
/* clang-format on */

/**************************************************************************
 * DAP Report Descriptor
 **************************************************************************/

static const uint8_t dap_report_desc[] = {
    /* Usage Page (Vendor Defined 0xFF00) */
    0x06, 0x00, 0xFF,
    /* Usage (Vendor Usage 1) */
    0x09, 0x01,
    /* Collection (Application) */
    0xA1, 0x01,

    /* --- Output Report (Host -> Device) --- */
    0x09, 0x02,            /* Usage */
    0x15, 0x00,            /* Logical Minimum (0) */
    0x26, 0xFF, 0x00,      /* Logical Maximum (255) */
    0x75, 0x08,            /* Report Size (8 bits) */
    0x95, HID_DAP_PACKET_SIZE, /* Report Count (64 bytes) */
    0x91, 0x02,            /* Output (Data, Var, Abs) */

    /* --- Input Report (Device -> Host) --- */
    0x09, 0x03,            /* Usage */
    0x15, 0x00,            /* Logical Minimum (0) */
    0x26, 0xFF, 0x00,      /* Logical Maximum (255) */
    0x75, 0x08,            /* Report Size (8 bits) */
    0x95, HID_DAP_PACKET_SIZE, /* Report Count (64 bytes) */
    0x81, 0x02,            /* Input (Data, Var, Abs) */

    /* End Collection */
    0xC0};

/**************************************************************************
 * USB Descriptors
 **************************************************************************/

static const struct usb_device_descriptor dap_dev_desc = {
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0210,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 64,
    .idVendor = DAP_VID,
    .idProduct = DAP_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

struct dap_config_desc
{
    struct usb_config_descriptor config;
    struct usb_interface_descriptor intf;
    struct usb_hid_descriptor hid;
    struct usb_endpoint_descriptor ep_out;
    struct usb_endpoint_descriptor ep_in;
} __attribute__((packed));

static const struct dap_config_desc dap_cfg_desc = {
    .config =
        {
            .bLength = USB_DT_CONFIG_SIZE,
            .bDescriptorType = USB_DT_CONFIG,
            .wTotalLength = sizeof(struct dap_config_desc),
            .bNumInterfaces = 1,
            .bConfigurationValue = 1,
            .iConfiguration = 0,
            .bmAttributes = 0x80, /* Bus-powered */
            .bMaxPower = 50,      /* 100mA */
        },
    .intf =
        {
            .bLength = USB_DT_INTERFACE_SIZE,
            .bDescriptorType = USB_DT_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 2,
            .bInterfaceClass = USB_CLASS_HID,
            .bInterfaceSubClass = 0x00,
            .bInterfaceProtocol = 0x00,
            .iInterface = 0,
        },
    .hid =
        {
            .bLength = USB_DT_HID_SIZE,
            .bDescriptorType = USB_DT_HID,
            .bcdHID = 0x0111,
            .bCountryCode = 0,
            .bNumDescriptors = 1,
            .bDescriptorType2 = USB_DT_REPORT,
            .wDescriptorLength = sizeof(dap_report_desc),
        },
    .ep_out =
        {
            .bLength = USB_DT_ENDPOINT_SIZE,
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = 0x01, /* EP1 OUT */
            .bmAttributes = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize = HID_DAP_PACKET_SIZE,
            .bInterval = 1,
        },
    .ep_in =
        {
            .bLength = USB_DT_ENDPOINT_SIZE,
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = 0x81, /* EP1 IN */
            .bmAttributes = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize = HID_DAP_PACKET_SIZE,
            .bInterval = 1,
        },
};

/**************************************************************************
 * String Descriptors
 **************************************************************************/

/* clang-format off */
static const uint8_t string0_desc[] = {0x04, USB_DT_STRING, 0x09, 0x04};
static const uint8_t string1_desc[] = {0x0A, USB_DT_STRING, 'R', 0, 'P', 0, 'I', 0, '5', 0};
static const uint8_t string2_desc[] = {0x1C, USB_DT_STRING, 
    'H', 0, 'I', 0, 'D', 0, ' ', 0, 'C', 0, 'M', 0, 'S',
     0, 'I', 0, 'S', 0, '-', 0, 'D', 0, 'A', 0, 'P', 0};
/* String 3: Serial Number - "0000001" = 7 chars, len = 2 + 7*2 = 16 = 0x10 */
static const uint8_t string3_desc[] = {0x10, USB_DT_STRING, 
                                      '0', 0, '0', 0, '0', 0, '0',
                                       0, '0', 0, '0', 0, '1', 0};
/* clang-format on */

static const uint8_t* dap_string_descs[] = {string1_desc, string2_desc, string3_desc};

/**************************************************************************
 * DAP Device State
 **************************************************************************/

struct virtual_dap
{
    struct usbip_usb_device udev;
    struct usb_control_context ctrl_ctx;
    struct hid_device_ctx hid_ctx;

    int exported;
    struct usbip_conn_ctx* ctx;

    /* DAP Response Buffer */
    uint8_t response[HID_DAP_PACKET_SIZE];
    size_t response_len;
    int response_pending;
    int response_valid;
};

static struct virtual_dap vdap;

/* DAP Command Processing */
static int dap_handle_data(uint8_t report_id, const void* data, size_t len, void* user_data);

/* HID Callback Implementation */
static int dap_get_report(uint8_t report_type, uint8_t report_id, void* data, size_t* len,
                          void* user_data);

/* URB and Device Management */
static int vdap_handle_urb(struct usbip_device_driver* driver, const struct usbip_header* urb_cmd,
                           struct usbip_header* urb_ret, void** data_out, size_t* data_len,
                           const void* urb_data, size_t urb_data_len);
static int vdap_get_device_count(struct usbip_device_driver* driver);
static int vdap_get_device_by_index(struct usbip_device_driver* driver, int index,
                                    struct usbip_usb_device* device);
static const struct usbip_usb_device* vdap_get_device(struct usbip_device_driver* driver,
                                                      const char* busid);
static int vdap_export_device(struct usbip_device_driver* driver, const char* busid,
                              struct usbip_conn_ctx* ctx);
static int vdap_unexport_device(struct usbip_device_driver* driver, const char* busid);
static int vdap_init(struct usbip_device_driver* driver);
static void vdap_cleanup(struct usbip_device_driver* driver);

/**************************************************************************
 * HID Callback Implementation
 **************************************************************************/

static const struct hid_device_ops dap_hid_ops = {
    .handle_data = dap_handle_data,
    .get_report = dap_get_report,
    .get_idle = NULL,
    .set_idle = NULL,
    .get_protocol = NULL,
    .set_protocol = NULL,
};

/**
 * dap_handle_data - Handle HID OUT data
 *
 * This function is called by the HID framework, data has been normalized with Report ID
 */
static int dap_handle_data(uint8_t report_id, const void* data, size_t len, void* user_data)
{
    (void)report_id;
    (void)user_data;

    if (!data || len == 0)
    {
        return -1;
    }

    LOG_HEX_DBG("[CMD] %zu bytes:", (const uint8_t*)data, len, len);
    vdap.response_len = DAP_ProcessCommand((const uint8_t*)data, vdap.response) & 0xFFFF;
    vdap.response_pending = 1;
    LOG_HEX_DBG("[RSP] %u bytes: ", vdap.response, vdap.response_len, vdap.response_len);

    return 0;
}

/**
 * dap_get_report - Get HID report
 *
 * Used to respond to GET_REPORT requests
 */
static int dap_get_report(uint8_t report_type, uint8_t report_id, void* data, size_t* len,
                          void* user_data)
{
    size_t copy_len;
    const size_t report_size = HID_DAP_PACKET_SIZE;

    (void)report_type;
    (void)report_id;
    (void)user_data;

    if (!data || !len)
    {
        return -1;
    }

    /* Return cached response data, fill remaining with 0 */
    if (vdap.response_pending)
    {
        copy_len = vdap.response_len < report_size ? vdap.response_len : report_size;
        memcpy(data, vdap.response, copy_len);
        if (copy_len < report_size)
        {
            memset((uint8_t*)data + copy_len, 0, report_size - copy_len);
        }
    }
    else
    {
        /* Return empty report when no response data */
        memset(data, 0, report_size);
    }

    *len = report_size;
    return 0;
}

/**************************************************************************
 * URB Processing
 **************************************************************************/

static int vdap_handle_urb(struct usbip_device_driver* driver, const struct usbip_header* urb_cmd,
                           struct usbip_header* urb_ret, void** data_out, size_t* data_len,
                           const void* urb_data, size_t urb_data_len)
{
    const struct usb_setup_packet* setup;
    uint32_t ep;
    int ret;

    (void)driver;
    ret = 1;

    if (!vdap.exported)
    {
        urb_ret->u.ret_submit.status = -1;
        return 1;
    }

    memset(urb_ret, 0, sizeof(*urb_ret));
    urb_ret->base.command = USBIP_RET_SUBMIT;
    urb_ret->base.seqnum = urb_cmd->base.seqnum;
    urb_ret->base.devid = urb_cmd->base.devid;
    urb_ret->base.direction = urb_cmd->base.direction;
    urb_ret->base.ep = urb_cmd->base.ep;
    ep = urb_cmd->base.ep;

    switch (urb_cmd->base.command)
    {
        case USBIP_CMD_SUBMIT:
            if (ep == 0)
            {
                setup = (const struct usb_setup_packet*)urb_cmd->u.cmd_submit.setup;

                /* Check if this is a Microsoft OS 2.0 vendor request */
                if (USB_SETUP_TYPE(setup) == 0x02 && USB_SETUP_IS_IN(setup) &&
                    setup->bRequest == MS_OS_20_VENDOR_CODE)
                {
                    /* MS_OS_20_DESCRIPTOR_INDEX */
                    if (setup->wIndex == 0x0007)
                    {
                        *data_out = osal_malloc(sizeof(dap_msos20_desc));
                        if (*data_out)
                        {
                            memcpy(*data_out, dap_msos20_desc, sizeof(dap_msos20_desc));
                            *data_len = sizeof(dap_msos20_desc);
                            if (*data_len > setup->wLength)
                            {
                                *data_len = setup->wLength;
                            }
                            urb_ret->u.ret_submit.actual_length = *data_len;
                            urb_ret->u.ret_submit.status = 0;
                        }
                        else
                        {
                            urb_ret->u.ret_submit.status = -ENOMEM;
                            urb_ret->u.ret_submit.actual_length = 0;
                        }
                        break;
                    }
                }

                if (USB_SETUP_IS_IN(setup))
                {
                    int ctrl_ret =
                        usb_control_handle_setup(setup, &vdap.ctrl_ctx, data_out, data_len);
                    if (ctrl_ret == USB_CONTROL_STALL)
                    {
                        urb_ret->u.ret_submit.status = -EPIPE;
                        urb_ret->u.ret_submit.actual_length = 0;
                    }
                    else
                    {
                        urb_ret->u.ret_submit.actual_length = *data_len;
                        urb_ret->u.ret_submit.status = 0;
                    }
                }
                else
                {
                    *data_out = NULL;
                    *data_len = 0;

                    if (urb_data && urb_data_len > 0 && USB_SETUP_TYPE(setup) == USB_TYPE_CLASS &&
                        USB_SETUP_RECIPIENT(setup) == USB_RECIP_INTERFACE &&
                        setup->bRequest == HID_REQUEST_SET_REPORT)
                    {
                        hid_handle_out_report(&vdap.hid_ctx, urb_data, urb_data_len);
                    }
                    urb_ret->u.ret_submit.actual_length = 0;
                    urb_ret->u.ret_submit.status = 0;
                }
            }
            else if (ep == 1 && urb_cmd->base.direction == USBIP_DIR_OUT)
            {
                *data_out = NULL;
                *data_len = 0;

                if (urb_data && urb_data_len > 0)
                {
                    hid_handle_out_report(&vdap.hid_ctx, urb_data, urb_data_len);
                }
                urb_ret->u.ret_submit.actual_length = urb_data_len;
                urb_ret->u.ret_submit.status = 0;
            }
            else if (ep == 1 && urb_cmd->base.direction == USBIP_DIR_IN)
            {
                if (vdap.response_pending)
                {
                    /* Return actual response length, not padded to 64 bytes */
                    *data_out = osal_malloc(vdap.response_len);
                    if (*data_out)
                    {
                        memcpy(*data_out, vdap.response, vdap.response_len);
                        *data_len = vdap.response_len;
                        urb_ret->u.ret_submit.actual_length = vdap.response_len;
                    }

                    urb_ret->u.ret_submit.status = 0;
                    vdap.response_pending = 0;
                    vdap.response_valid = 0;
                    LOG_DBG("IN: %zu bytes", vdap.response_len);
                }
                else
                {
                    /* Delay 1ms when no response to optimize PyOCD response */
                    osal_sleep_ms(1);
                    *data_out = NULL;
                    *data_len = 0;
                    urb_ret->u.ret_submit.status = 0;
                    urb_ret->u.ret_submit.actual_length = 0;
                }
            }
            else
            {
                *data_out = NULL;
                *data_len = 0;
                urb_ret->u.ret_submit.status = 0;
                urb_ret->u.ret_submit.actual_length = 0;
            }
            break;

        case USBIP_CMD_UNLINK:
            urb_ret->base.command = USBIP_RET_UNLINK;
            urb_ret->u.ret_unlink.status = 0;
            break;

        default:
            ret = -1;
            break;
    }

    return ret;
}

/**************************************************************************
 * Device Enumeration Interface
 **************************************************************************/

static int vdap_get_device_count(struct usbip_device_driver* driver)
{
    (void)driver;

    if (vdap.udev.busid[0] == '\0' || usbip_is_device_busy(vdap.udev.busid))
    {
        return 0;
    }

    return 1;
}

static int vdap_get_device_by_index(struct usbip_device_driver* driver, int index,
                                    struct usbip_usb_device* device)
{
    (void)driver;

    if (index != 0 || vdap.udev.busid[0] == '\0' || usbip_is_device_busy(vdap.udev.busid))
    {
        return -1;
    }

    memcpy(device, &vdap.udev, sizeof(*device));

    return 0;
}

static const struct usbip_usb_device* vdap_get_device(struct usbip_device_driver* driver,
                                                      const char* busid)
{
    (void)driver;

    if (strcmp(vdap.udev.busid, busid) == 0)
    {
        return &vdap.udev;
    }
    return NULL;
}

static int vdap_export_device(struct usbip_device_driver* driver, const char* busid,
                              struct usbip_conn_ctx* ctx)
{
    (void)driver;

    if (strcmp(vdap.udev.busid, busid) != 0 || vdap.exported)
    {
        return -1;
    }

    vdap.exported = 1;
    vdap.ctx = ctx;
    usbip_set_device_busy(busid);
    /* Set DAP packet size for this device */
    DAP_SetPacketSize(HID_DAP_PACKET_SIZE);

    LOG_INF("Exported: %s", busid);
    return 0;
}

static int vdap_unexport_device(struct usbip_device_driver* driver, const char* busid)
{
    (void)driver;

    if (strcmp(vdap.udev.busid, busid) != 0)
    {
        return -1;
    }

    vdap.exported = 0;
    vdap.ctx = NULL;
    usbip_set_device_available(busid);

    LOG_INF("Unexported: %s", busid);
    return 0;
}

/**************************************************************************
 * Initialization and Cleanup
 **************************************************************************/

static int vdap_init(struct usbip_device_driver* driver)
{
    (void)driver;

    memset(&vdap, 0, sizeof(vdap));

    strncpy(vdap.udev.path, "/sys/devices/platform/virtual-dap", SYSFS_PATH_MAX - 1);
    strncpy(vdap.udev.busid, "2-1", SYSFS_BUS_ID_SIZE - 1);
    vdap.udev.busnum = 1;
    vdap.udev.devnum = 3;
    vdap.udev.speed = USB_SPEED_FULL;
    vdap.udev.idVendor = dap_dev_desc.idVendor;
    vdap.udev.idProduct = dap_dev_desc.idProduct;
    vdap.udev.bcdDevice = dap_dev_desc.bcdDevice;
    vdap.udev.bDeviceClass = dap_dev_desc.bDeviceClass;
    vdap.udev.bDeviceSubClass = dap_dev_desc.bDeviceSubClass;
    vdap.udev.bDeviceProtocol = dap_dev_desc.bDeviceProtocol;
    vdap.udev.bConfigurationValue = 1;
    vdap.udev.bNumConfigurations = dap_dev_desc.bNumConfigurations;
    vdap.udev.bNumInterfaces = 1;

    vdap.ctrl_ctx = (struct usb_control_context)USB_CONTROL_CONTEXT_INIT(
        &dap_dev_desc, &dap_cfg_desc, sizeof(dap_cfg_desc));
    vdap.ctrl_ctx.hid_desc = &dap_cfg_desc.hid;
    vdap.ctrl_ctx.report_desc = dap_report_desc;
    vdap.ctrl_ctx.report_desc_len = sizeof(dap_report_desc);
    vdap.ctrl_ctx.lang_id_desc = string0_desc;
    vdap.ctrl_ctx.string_descs = dap_string_descs;
    vdap.ctrl_ctx.num_strings = 3;
    vdap.ctrl_ctx.num_configs = 1;
    vdap.ctrl_ctx.bos_desc = dap_bos_desc;
    vdap.ctrl_ctx.bos_desc_len = sizeof(dap_bos_desc);
    hid_init_ctx(&vdap.hid_ctx, &dap_hid_ops, HID_DAP_PACKET_SIZE, &vdap);
    DAP_Setup();

    LOG_DBG("Init (VID=%04x PID=%04x)", DAP_VID, DAP_PID);
    return 0;
}

static void vdap_cleanup(struct usbip_device_driver* driver)
{
    (void)driver;
    memset(&vdap, 0, sizeof(vdap));
}

/**************************************************************************
 * Driver Definition
 **************************************************************************/

struct usbip_device_driver virtual_dap_driver = {
    .name = "virtual-dap",
    .get_device_count = vdap_get_device_count,
    .get_device_by_index = vdap_get_device_by_index,
    .get_device = vdap_get_device,
    .export_device = vdap_export_device,
    .unexport_device = vdap_unexport_device,
    .handle_urb = vdap_handle_urb,
    .init = vdap_init,
    .cleanup = vdap_cleanup,
};

/*****************************************************************************
 * Auto-Register on Program Startup
 *****************************************************************************/

void __attribute__((constructor, used)) hid_dap_driver_register(void)
{
    usbip_register_driver(&virtual_dap_driver);
}