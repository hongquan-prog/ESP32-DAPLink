/**
 * @file USB_handle.c
 * @brief Handle all Standard Device Requests on endpoint 0
 * @version 0.1
 * @date 2020-01-23
 *
 * @copyright Copyright (c) 2020
 *
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "components/USBIP/usb_handle.h"
#include "components/USBIP/usb_descriptor.h"
#include "components/USBIP/MSOS20_descriptor.h"

const char *strings_list[] = {
        0, // reserved: available languages  -> iInterface
        "windowsair",
        "Wireless ESP CMSIS-DAP",
        "1234",
};

/* Helper macros for callbacks */
#define SEND_SUBMIT_DATA(ops, header, status, data, len) \
    (ops)->send_submit_data((ops)->user_data, header, status, data, len)

#define SEND_SUBMIT(ops, header, status, len) \
    (ops)->send_submit((ops)->user_data, header, status, len)

#define SEND_DATA(ops, data, len) \
    (ops)->send_data((ops)->user_data, data, len)

/* Forward declarations */
static void handleGetDescriptor(usbip_stage2_header *header, const usb_send_ops_t *ops);

void handleUSBControlRequest(usbip_stage2_header *header, const usb_send_ops_t *ops)
{
    /* Table 9-3. Standard Device Requests */
    switch (header->u.cmd_submit.request.bmRequestType)
    {
    case 0x00: /* ignore.. */
        switch (header->u.cmd_submit.request.bRequest)
        {
        case USB_REQ_CLEAR_FEATURE:
            printf("* CLEAR FEATURE\r\n");
            SEND_SUBMIT_DATA(ops, header, 0, 0, 0);
            break;
        case USB_REQ_SET_FEATURE:
            printf("* SET FEATURE\r\n");
            SEND_SUBMIT_DATA(ops, header, 0, 0, 0);
            break;
        case USB_REQ_SET_ADDRESS:
            printf("* SET ADDRESS\r\n");
            SEND_SUBMIT_DATA(ops, header, 0, 0, 0);
            break;
        case USB_REQ_SET_DESCRIPTOR:
            printf("* SET DESCRIPTOR\r\n");
            SEND_SUBMIT_DATA(ops, header, 0, 0, 0);
            break;
        case USB_REQ_SET_CONFIGURATION:
            printf("* SET CONFIGURATION\r\n");
            SEND_SUBMIT_DATA(ops, header, 0, 0, 0);
            break;
        default:
            printf("USB unknown request, bmRequestType:%d,bRequest:%d\r\n",
                      header->u.cmd_submit.request.bmRequestType, header->u.cmd_submit.request.bRequest);
            break;
        }
        break;
    case 0x01: /* ignore... */
        switch (header->u.cmd_submit.request.bRequest)
        {
        case USB_REQ_CLEAR_FEATURE:
            printf("* CLEAR FEATURE\r\n");
            SEND_SUBMIT_DATA(ops, header, 0, 0, 0);
            break;
        case USB_REQ_SET_FEATURE:
            printf("* SET FEATURE\r\n");
            SEND_SUBMIT_DATA(ops, header, 0, 0, 0);
            break;
        case USB_REQ_SET_INTERFACE:
            printf("* SET INTERFACE\r\n");
            SEND_SUBMIT_DATA(ops, header, 0, 0, 0);
            break;

        default:
            printf("USB unknown request, bmRequestType:%d,bRequest:%d\r\n",
                      header->u.cmd_submit.request.bmRequestType, header->u.cmd_submit.request.bRequest);
            break;
        }
        break;
    case 0x02: /* ignore.. */
        switch (header->u.cmd_submit.request.bRequest)
        {
        case USB_REQ_CLEAR_FEATURE:
            printf("* CLEAR FEATURE\r\n");
            SEND_SUBMIT_DATA(ops, header, 0, 0, 0);
            break;
        case USB_REQ_SET_FEATURE:
            printf("* SET INTERFACE\r\n");
            SEND_SUBMIT_DATA(ops, header, 0, 0, 0);
            break;

        default:
            printf("USB unknown request, bmRequestType:%d,bRequest:%d\r\n",
                      header->u.cmd_submit.request.bmRequestType, header->u.cmd_submit.request.bRequest);
            break;
        }
        break;

    case 0x80: /* *IMPORTANT* */
#if (USBIP_ENABLE_WINUSB == 0)
    case 0x81:
#endif
    {
        switch (header->u.cmd_submit.request.bRequest)
        {
        case USB_REQ_GET_CONFIGURATION:
            printf("* GET CONIFGTRATION\r\n");
            SEND_SUBMIT_DATA(ops, header, 0, 0, 0);
            break;
        case USB_REQ_GET_DESCRIPTOR:
            handleGetDescriptor(header, ops);
            break;
        case USB_REQ_GET_STATUS:
            printf("* GET STATUS\r\n");
            SEND_SUBMIT_DATA(ops, header, 0, 0, 0);
            break;
        default:
            printf("USB unknown request, bmRequestType:%d,bRequest:%d\r\n",
                      header->u.cmd_submit.request.bmRequestType, header->u.cmd_submit.request.bRequest);
            break;
        }
        break;
    }
#if (USBIP_ENABLE_WINUSB == 1)
    case 0x81: /* ignore... */
        switch (header->u.cmd_submit.request.bRequest)
        {
        case USB_REQ_GET_INTERFACE:
            printf("* GET INTERFACE\r\n");
            SEND_SUBMIT_DATA(ops, header, 0, 0, 0);
            break;
        case USB_REQ_SET_SYNCH_FRAME:
            printf("* SET SYNCH FRAME\r\n");
            SEND_SUBMIT_DATA(ops, header, 0, 0, 0);
            break;
        case USB_REQ_GET_STATUS:
            printf("* GET STATUS\r\n");
            SEND_SUBMIT_DATA(ops, header, 0, 0, 0);
            break;

        default:
            printf("USB unknown request, bmRequestType:%d,bRequest:%d\r\n",
                      header->u.cmd_submit.request.bmRequestType, header->u.cmd_submit.request.bRequest);
            break;
        }
        break;
#endif
    case 0x82: /* ignore... */
        switch (header->u.cmd_submit.request.bRequest)
        {
        case USB_REQ_GET_STATUS:
            printf("* GET STATUS\r\n");
            SEND_SUBMIT_DATA(ops, header, 0, 0, 0);
            break;

        default:
            printf("USB unknown request, bmRequestType:%d,bRequest:%d\r\n",
                      header->u.cmd_submit.request.bmRequestType, header->u.cmd_submit.request.bRequest);
            break;
        }
        break;
    case 0xC0: /* Microsoft OS 2.0 vendor-specific descriptor */
    {
        uint16_t wIndex = header->u.cmd_submit.request.wIndex.u16;
        switch (wIndex)
        {
        case MS_OS_20_DESCRIPTOR_INDEX:
            printf("* GET MSOS 2.0 vendor-specific descriptor\r\n");
            SEND_SUBMIT_DATA(ops, header, 0, msOs20DescriptorSetHeader, sizeof(msOs20DescriptorSetHeader));
            break;
        case MS_OS_20_SET_ALT_ENUMERATION:
            /* set alternate enumeration command */
            /* bAltEnumCode set to 0 */
            printf("Set alternate enumeration.This should not happen.\r\n");
            break;

        default:
            printf("USB unknown request, bmRequestType:%d,bRequest:%d,wIndex:%d\r\n",
                      header->u.cmd_submit.request.bmRequestType, header->u.cmd_submit.request.bRequest, wIndex);
            break;
        }
        break;
    }
    case 0x21: /* Set_Idle for HID */
        switch (header->u.cmd_submit.request.bRequest)
        {
        case USB_REQ_SET_IDLE:
            printf("* SET IDLE\r\n");
            SEND_SUBMIT(ops, header, 0, 0);
            break;

        default:
            printf("USB unknown request, bmRequestType:%d,bRequest:%d\r\n",
                      header->u.cmd_submit.request.bmRequestType, header->u.cmd_submit.request.bRequest);
            break;
        }
        break;
    default:
        printf("USB unknown request, bmRequestType:%d,bRequest:%d\r\n",
                  header->u.cmd_submit.request.bmRequestType, header->u.cmd_submit.request.bRequest);
        break;
    }
}

static void handleGetDescriptor(usbip_stage2_header *header, const usb_send_ops_t *ops)
{
    /* 9.4.3 Get Descriptor */
    switch (header->u.cmd_submit.request.wValue.u8hi)
    {
    case USB_DT_DEVICE: /* get device descriptor */
        printf("* GET 0x01 DEVICE DESCRIPTOR\r\n");
        SEND_SUBMIT_DATA(ops, header, 0, &kUSBd0DeviceDescriptor[0], sizeof(kUSBd0DeviceDescriptor));
        break;

    case USB_DT_CONFIGURATION: /* get configuration descriptor */
        printf("* GET 0x02 CONFIGURATION DESCRIPTOR\r\n");
        {
            if (header->u.cmd_submit.data_length == USB_DT_CONFIGURATION_SIZE)
            {
                printf("Sending only first part of CONFIG\r\n");

                SEND_SUBMIT(ops, header, 0, header->u.cmd_submit.data_length);
                SEND_DATA(ops, kUSBd0ConfigDescriptor, sizeof(kUSBd0ConfigDescriptor));
            }
            else
            {
                printf("Sending ALL CONFIG\r\n");
                SEND_SUBMIT(ops, header, 0, sizeof(kUSBd0ConfigDescriptor) + sizeof(kUSBd0InterfaceDescriptor));
                SEND_DATA(ops, kUSBd0ConfigDescriptor, sizeof(kUSBd0ConfigDescriptor));
                SEND_DATA(ops, kUSBd0InterfaceDescriptor, sizeof(kUSBd0InterfaceDescriptor));
            }
        }
        break;

    case USB_DT_STRING:
        if (header->u.cmd_submit.request.wValue.u8lo == 0)
        {
            printf("** REQUESTED list of supported languages\r\n");
            SEND_SUBMIT_DATA(ops, header, 0, kLangDescriptor, sizeof(kLangDescriptor));
        }
        else if (header->u.cmd_submit.request.wValue.u8lo != 0xee)
        {
            int slen = strlen(strings_list[header->u.cmd_submit.request.wValue.u8lo]);
            int wslen = slen * 2;
            int buff_len = sizeof(usb_string_descriptor) + wslen;
            char temp_buff[64];
            usb_string_descriptor *desc = (usb_string_descriptor *)temp_buff;
            desc->bLength = buff_len;
            desc->bDescriptorType = USB_DT_STRING;
            for (int i = 0; i < slen; i++)
            {
                desc->wData[i] = strings_list[header->u.cmd_submit.request.wValue.u8lo][i];
            }
            SEND_SUBMIT_DATA(ops, header, 0, (uint8_t *)temp_buff, buff_len);
        }
        else
        {
            printf("low bit : %d\r\n", (int)header->u.cmd_submit.request.wValue.u8lo);
            printf("high bit : %d\r\n", (int)header->u.cmd_submit.request.wValue.u8hi);
            printf("***Unsupported String descriptor***\r\n");
            SEND_SUBMIT(ops, header, 0, 0);
        }
        break;

    case USB_DT_INTERFACE:
        printf("* GET 0x04 INTERFACE DESCRIPTOR (UNIMPLEMENTED)\r\n");
        SEND_SUBMIT(ops, header, 0, 0);
        break;

    case USB_DT_ENDPOINT:
        printf("* GET 0x05 ENDPOINT DESCRIPTOR (UNIMPLEMENTED)\r\n");
        SEND_SUBMIT(ops, header, 0, 0);
        break;

    case USB_DT_DEVICE_QUALIFIER:
        printf("* GET 0x06 DEVICE QUALIFIER DESCRIPTOR\r\n");

        usb_device_qualifier_descriptor desc;

        memset(&desc, 0, sizeof(usb_device_qualifier_descriptor));

        SEND_SUBMIT_DATA(ops, header, 0, &desc, sizeof(usb_device_qualifier_descriptor));
        break;

    case USB_DT_OTHER_SPEED_CONFIGURATION:
        printf("GET 0x07 [UNIMPLEMENTED] USB_DT_OTHER_SPEED_CONFIGURATION\r\n");
        SEND_SUBMIT(ops, header, 0, 0);
        break;

    case USB_DT_INTERFACE_POWER:
        printf("GET 0x08 [UNIMPLEMENTED] USB_DT_INTERFACE_POWER\r\n");
        SEND_SUBMIT(ops, header, 0, 0);
        break;
#if (USBIP_ENABLE_WINUSB == 1)
    case USB_DT_BOS:
        printf("* GET 0x0F BOS DESCRIPTOR\r\n");
        uint32_t bos_len = header->u.cmd_submit.request.wLength.u8lo | ((uint32_t) header->u.cmd_submit.request.wLength.u8hi << 8);
        bos_len = (sizeof(bosDescriptor) < bos_len) ? sizeof(bosDescriptor) : bos_len;
        SEND_SUBMIT_DATA(ops, header, 0, bosDescriptor, bos_len);
        break;
#else
    case USB_DT_HID_REPORT:
        printf("* GET 0x22 HID REPORT DESCRIPTOR\r\n");
        SEND_SUBMIT_DATA(ops, header, 0, (void *)kHidReportDescriptor, sizeof(kHidReportDescriptor));
        break;
#endif
    default:
        printf("USB unknown Get Descriptor requested:%d\r\n", header->u.cmd_submit.request.wValue.u8lo);
        printf("low bit :%d\r\n",header->u.cmd_submit.request.wValue.u8lo);
        printf("high bit :%d\r\n",header->u.cmd_submit.request.wValue.u8hi);
        break;
    }
}