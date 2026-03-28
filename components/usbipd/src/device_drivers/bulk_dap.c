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
 * Virtual CMSIS-DAP v2 Bulk Device
 *
 * Virtual CMSIS-DAP v2 debug probe via USBIP
 * Uses Bulk endpoint transfer, matching real DAPLink v2 behavior
 */

#include <endian.h>
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
#include "usbip_protocol.h"
#include "usbip_server.h"

LOG_MODULE_REGISTER(dap_v2, CONFIG_DAP_LOG_LEVEL);

/*****************************************************************************
 * DAP v2 Device Configuration
 *****************************************************************************/

#define BULK_DAP_VID 0xFAED
#define BULK_DAP_PID 0x4873
#define BULK_DAP_PACKET_SIZE 512 /* High Speed Bulk packet size */

/* Compile-time check: BULK_DAP_PACKET_SIZE must not exceed USBIP_URB_DATA_MAX_SIZE */
#if BULK_DAP_PACKET_SIZE > USBIP_URB_DATA_MAX_SIZE
#error "BULK_DAP_PACKET_SIZE exceeds USBIP_URB_DATA_MAX_SIZE. Please increase USBIP_URB_DATA_MAX_SIZE in include/usbip_server.h"
#endif

/*****************************************************************************
 * BOS Descriptor - Contains Microsoft OS 2.0 Platform Capability
 *****************************************************************************/

#define MS_OS_20_VENDOR_CODE 0x01
#define MS_OS_20_SET_LEN 0xA2

static const uint8_t dap_v2_bos_desc[] = {
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

/*****************************************************************************
 * Microsoft OS 2.0 Descriptor Set - Follow reference code exactly
 *****************************************************************************/

/* clang-format off */
static const uint8_t dap_v2_msos20_desc[] = {
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

/*****************************************************************************
 * USB Descriptors
 *****************************************************************************/

static const struct usb_device_descriptor dap_v2_dev_desc = {
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0210,     /* USB 2.10 */
    .bDeviceClass = 0x00, /* Defined at interface level */
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 64,
    .idVendor = BULK_DAP_VID,
    .idProduct = BULK_DAP_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

/* Interface Association Descriptor (IAD) */
struct usb_iad
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bFirstInterface;
    uint8_t bInterfaceCount;
    uint8_t bFunctionClass;
    uint8_t bFunctionSubClass;
    uint8_t bFunctionProtocol;
    uint8_t iFunction;
} __attribute__((packed));

/* CDC Class Descriptor */
struct cdc_header_desc
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint16_t bcdCDC;
} __attribute__((packed));

struct cdc_call_mgmt_desc
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bmCapabilities;
    uint8_t bDataInterface;
} __attribute__((packed));

struct cdc_acm_desc
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bmCapabilities;
} __attribute__((packed));

struct cdc_union_desc
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bMasterInterface;
    uint8_t bSlaveInterface;
} __attribute__((packed));

/* DAP v2 Simple Config Descriptor - Single Interface */
struct dap_v2_config_desc
{
    struct usb_config_descriptor config;
    struct usb_interface_descriptor dap_intf;
    struct usb_endpoint_descriptor dap_ep_out;
    struct usb_endpoint_descriptor dap_ep_in;
} __attribute__((packed));

static const struct dap_v2_config_desc dap_v2_cfg_desc = {
    .config =
        {
            .bLength = USB_DT_CONFIG_SIZE,
            .bDescriptorType = USB_DT_CONFIG,
            .wTotalLength = sizeof(struct dap_v2_config_desc),
            .bNumInterfaces = 1,
            .bConfigurationValue = 1,
            .iConfiguration = 0,
            .bmAttributes = 0x80,
            .bMaxPower = 250, /* 500mA */
        },

    /* DAP v2 Interface */
    .dap_intf =
        {
            .bLength = USB_DT_INTERFACE_SIZE,
            .bDescriptorType = USB_DT_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 2,
            .bInterfaceClass = 0xFF, /* Vendor Specific */
            .bInterfaceSubClass = 0x00,
            .bInterfaceProtocol = 0x00,
            .iInterface = 4,
        },
    .dap_ep_out =
        {
            .bLength = USB_DT_ENDPOINT_SIZE,
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = 0x01,
            .bmAttributes = 0x02, /* Bulk */
            .wMaxPacketSize = BULK_DAP_PACKET_SIZE,
            .bInterval = 0,
        },
    .dap_ep_in =
        {
            .bLength = USB_DT_ENDPOINT_SIZE,
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = 0x81,
            .bmAttributes = 0x02, /* Bulk */
            .wMaxPacketSize = BULK_DAP_PACKET_SIZE,
            .bInterval = 0,
        },
};

/*****************************************************************************
 * String Descriptors
 *****************************************************************************/

/* clang-format off */
static const uint8_t string0_desc[] = { 
    0x04, USB_DT_STRING, 0x09, 0x04 };
static const uint8_t string1_desc[] = { 
    0x0A, USB_DT_STRING,
    'R', 0, 'P', 0, 'I', 0, '5', 0 };
static const uint8_t string2_desc[] = { 0x1C, USB_DT_STRING,
    'B', 0, 'L', 0, 'K', 0, ' ', 0, 'C', 0, 'M', 0,
    'S', 0, 'I', 0, 'S', 0, '-', 0, 'D', 0, 'A', 0, 'P', 0 };
static const uint8_t string3_desc[] = { 0x14, USB_DT_STRING,
    '1', 0, '2', 0, '3', 0, '4', 0, '5', 0, '6', 0, '7', 0, '8', 0, '9', 0, '0', 0 };
static const uint8_t string4_desc[] = { 0x14, USB_DT_STRING,
    'C', 0, 'M', 0, 'S', 0, 'I', 0, 'S', 0, '-', 0, 'D', 0, 'A', 0, 'P', 0, ' ', 0, 'v', 0, '2', 0 };
/* clang-format on */

static const uint8_t* dap_v2_string_descs[] = {string1_desc, string2_desc, string3_desc,
                                               string4_desc};

/*****************************************************************************
 * DAP v2 Device State
 *****************************************************************************/

struct virtual_dap_v2
{
    struct usbip_usb_device udev;
    struct usb_control_context ctrl_ctx;

    int exported;
    struct usbip_conn_ctx* ctx;

    /* DAP Response Buffer */
    uint8_t response[BULK_DAP_PACKET_SIZE];
    size_t response_len;
    int response_pending;
};

static struct virtual_dap_v2 vdap_v2;

static int vdap_v2_handle_urb(struct usbip_device_driver* driver,
                              const struct usbip_header* urb_cmd, struct usbip_header* urb_ret,
                              void** data_out, size_t* data_len, const void* urb_data,
                              size_t urb_data_len);
static int vdap_v2_get_device_count(struct usbip_device_driver* driver);
static int vdap_v2_get_device_by_index(struct usbip_device_driver* driver, int index,
                                       struct usbip_usb_device* device);
static const struct usbip_usb_device* vdap_v2_get_device(struct usbip_device_driver* driver,
                                                         const char* busid);
static int vdap_v2_export_device(struct usbip_device_driver* driver, const char* busid,
                                 struct usbip_conn_ctx* ctx);
static int vdap_v2_unexport_device(struct usbip_device_driver* driver, const char* busid);
static int vdap_v2_init(struct usbip_device_driver* driver);
static void vdap_v2_cleanup(struct usbip_device_driver* driver);

/*****************************************************************************
 * DAP Command Processing
 *****************************************************************************/

static void dap_v2_process_command(const void* data, size_t len)
{
    LOG_HEX_DBG("[CMD] %zu bytes:", (const uint8_t*)data, len, len);
    vdap_v2.response_len = DAP_ProcessCommand((const uint8_t*)data, vdap_v2.response) & 0xFFFF;
    vdap_v2.response_pending = 1;
    LOG_HEX_DBG("[RSP] %zu bytes:", vdap_v2.response, vdap_v2.response_len, vdap_v2.response_len);
}

/*****************************************************************************
 * URB Processing
 *****************************************************************************/

static int vdap_v2_handle_urb(struct usbip_device_driver* driver,
                              const struct usbip_header* urb_cmd, struct usbip_header* urb_ret,
                              void** data_out, size_t* data_len, const void* urb_data,
                              size_t urb_data_len)
{
    uint32_t ep;
    int ret;

    (void)driver;
    ret = 1;

    if (!vdap_v2.exported)
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
                /* Control endpoint */
                const struct usb_setup_packet* setup =
                    (const struct usb_setup_packet*)urb_cmd->u.cmd_submit.setup;

                /* Log all control requests */
                LOG_DBG("Control: bmRequestType=0x%02x bRequest=0x%02x wValue=0x%04x wIndex=0x%04x "
                        "wLength=%d",
                        setup->bmRequestType, setup->bRequest, setup->wValue, setup->wIndex,
                        setup->wLength);

                /* Check if this is a Microsoft OS 2.0 vendor request */
                if (USB_SETUP_TYPE(setup) == 0x02 && /* Vendor type */
                    USB_SETUP_IS_IN(setup))
                {
                    LOG_DBG("Vendor IN request: bRequest=0x%02x wIndex=0x%04x wValue=0x%04x",
                            setup->bRequest, setup->wIndex, setup->wValue);

                    if (setup->bRequest == MS_OS_20_VENDOR_CODE)
                    {
                        /* MS_OS_20_DESCRIPTOR_INDEX */
                        if (setup->wIndex == 0x0007)
                        {
                            LOG_DBG("MS OS 2.0 Descriptor request");
                            *data_out = osal_malloc(sizeof(dap_v2_msos20_desc));
                            if (*data_out)
                            {
                                memcpy(*data_out, dap_v2_msos20_desc, sizeof(dap_v2_msos20_desc));
                                *data_len = sizeof(dap_v2_msos20_desc);
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
                        /* MS_OS_20_SET_ALT_ENUMERATION */
                        else if (setup->wIndex == 0x0008)
                        {
                            LOG_INF("MS OS 2.0 Alt Enumeration request");
                            *data_out = NULL;
                            *data_len = 0;
                            urb_ret->u.ret_submit.actual_length = 0;
                            urb_ret->u.ret_submit.status = 0;
                            break;
                        }
                    }
                }

                if (USB_SETUP_IS_IN(setup))
                {
                    int ctrl_ret =
                        usb_control_handle_setup(setup, &vdap_v2.ctrl_ctx, data_out, data_len);
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
                    urb_ret->u.ret_submit.actual_length = 0;
                    urb_ret->u.ret_submit.status = 0;
                }
            }
            /* DAP v2 Bulk OUT (EP1) */
            else if (ep == 1 && urb_cmd->base.direction == USBIP_DIR_OUT)
            {
                *data_out = NULL;
                *data_len = 0;

                if (urb_data && urb_data_len > 0)
                {
                    dap_v2_process_command(urb_data, urb_data_len);
                }
                urb_ret->u.ret_submit.actual_length = urb_data_len;
                urb_ret->u.ret_submit.status = 0;
            }
            /* DAP v2 Bulk IN (EP1) */
            else if (ep == 1 && urb_cmd->base.direction == USBIP_DIR_IN)
            {
                if (vdap_v2.response_pending)
                {
                    size_t actual_len = vdap_v2.response_len;
                    *data_out = osal_malloc(actual_len);
                    if (*data_out)
                    {
                        memcpy(*data_out, vdap_v2.response, actual_len);
                        *data_len = actual_len;
                        urb_ret->u.ret_submit.actual_length = actual_len;
                    }
                    urb_ret->u.ret_submit.status = 0;
                    vdap_v2.response_pending = 0;
                    LOG_DBG("IN: %zu bytes", actual_len);
                }
                else
                {
                    /* Bulk endpoint has no data, return NAK (return 0 bytes to let host retry) */
                    *data_out = NULL;
                    *data_len = 0;
                    urb_ret->u.ret_submit.status = 0;
                    urb_ret->u.ret_submit.actual_length = 0;
                    /* Delay 1ms when no response to optimize PyOCD response */
                    osal_sleep_ms(1);
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
            LOG_ERR("Unknown command: 0x%04x", urb_cmd->base.command);
            ret = -1;
            break;
    }

    return ret;
}

/*****************************************************************************
 * Device Enumeration Interface
 *****************************************************************************/

static int vdap_v2_get_device_count(struct usbip_device_driver* driver)
{
    (void)driver;

    if (vdap_v2.udev.busid[0] == '\0' || usbip_is_device_busy(vdap_v2.udev.busid))
    {
        return 0;
    }

    return 1;
}

static int vdap_v2_get_device_by_index(struct usbip_device_driver* driver, int index,
                                       struct usbip_usb_device* device)
{
    (void)driver;

    if (index != 0 || vdap_v2.udev.busid[0] == '\0' ||
        usbip_is_device_busy(vdap_v2.udev.busid))
    {
        return -1;
    }

    memcpy(device, &vdap_v2.udev, sizeof(*device));

    return 0;
}

static const struct usbip_usb_device* vdap_v2_get_device(struct usbip_device_driver* driver,
                                                         const char* busid)
{
    (void)driver;

    if (strcmp(vdap_v2.udev.busid, busid) == 0)
    {
        return &vdap_v2.udev;
    }
    return NULL;
}

static int vdap_v2_export_device(struct usbip_device_driver* driver, const char* busid,
                                 struct usbip_conn_ctx* ctx)
{
    (void)driver;

    if (strcmp(vdap_v2.udev.busid, busid) != 0 || vdap_v2.exported)
    {
        return -1;
    }

    vdap_v2.exported = 1;
    vdap_v2.ctx = ctx;
    usbip_set_device_busy(busid);
    /* Set DAP packet size for this device */
    DAP_SetPacketSize(BULK_DAP_PACKET_SIZE);

    LOG_INF("Exported: %s", busid);
    return 0;
}

static int vdap_v2_unexport_device(struct usbip_device_driver* driver, const char* busid)
{
    (void)driver;

    if (strcmp(vdap_v2.udev.busid, busid) != 0)
    {
        return -1;
    }

    vdap_v2.exported = 0;
    vdap_v2.ctx = NULL;
    usbip_set_device_available(busid);

    LOG_INF("Unexported: %s", busid);
    return 0;
}

/*****************************************************************************
 * Initialization and Cleanup
 *****************************************************************************/

static int vdap_v2_init(struct usbip_device_driver* driver)
{
    (void)driver;

    memset(&vdap_v2, 0, sizeof(vdap_v2));

    strncpy(vdap_v2.udev.path, "/sys/devices/platform/virtual-dap-v2", SYSFS_PATH_MAX - 1);
    strncpy(vdap_v2.udev.busid, "2-2", SYSFS_BUS_ID_SIZE - 1);
    vdap_v2.udev.busnum = 1;
    vdap_v2.udev.devnum = 4;
    vdap_v2.udev.speed = USB_SPEED_HIGH;
    vdap_v2.udev.idVendor = dap_v2_dev_desc.idVendor;
    vdap_v2.udev.idProduct = dap_v2_dev_desc.idProduct;
    vdap_v2.udev.bcdDevice = dap_v2_dev_desc.bcdDevice;
    vdap_v2.udev.bDeviceClass = dap_v2_dev_desc.bDeviceClass;
    vdap_v2.udev.bDeviceSubClass = dap_v2_dev_desc.bDeviceSubClass;
    vdap_v2.udev.bDeviceProtocol = dap_v2_dev_desc.bDeviceProtocol;
    vdap_v2.udev.bConfigurationValue = 1;
    vdap_v2.udev.bNumConfigurations = dap_v2_dev_desc.bNumConfigurations;
    vdap_v2.udev.bNumInterfaces = 1;

    vdap_v2.ctrl_ctx = (struct usb_control_context)USB_CONTROL_CONTEXT_INIT(
        &dap_v2_dev_desc, &dap_v2_cfg_desc, sizeof(dap_v2_cfg_desc));
    vdap_v2.ctrl_ctx.lang_id_desc = string0_desc;
    vdap_v2.ctrl_ctx.string_descs = dap_v2_string_descs;
    vdap_v2.ctrl_ctx.num_strings = 4;
    vdap_v2.ctrl_ctx.num_configs = 1;
    vdap_v2.ctrl_ctx.bos_desc = dap_v2_bos_desc;
    vdap_v2.ctrl_ctx.bos_desc_len = sizeof(dap_v2_bos_desc);

    DAP_Setup();

    LOG_INF("Init (VID=%04x PID=%04x) Bulk mode", BULK_DAP_VID, BULK_DAP_PID);
    return 0;
}

static void vdap_v2_cleanup(struct usbip_device_driver* driver)
{
    (void)driver;
    memset(&vdap_v2, 0, sizeof(vdap_v2));
}

struct usbip_device_driver virtual_dap_v2_driver = {
    .name = "virtual-dap-v2",
    .get_device_count = vdap_v2_get_device_count,
    .get_device_by_index = vdap_v2_get_device_by_index,
    .get_device = vdap_v2_get_device,
    .export_device = vdap_v2_export_device,
    .unexport_device = vdap_v2_unexport_device,
    .handle_urb = vdap_v2_handle_urb,
    .init = vdap_v2_init,
    .cleanup = vdap_v2_cleanup,
};

/*****************************************************************************
 * Auto-Register on Program Startup
 *****************************************************************************/

void __attribute__((constructor, used)) bulk_dap_driver_register(void)
{
    usbip_register_driver(&virtual_dap_v2_driver);
}