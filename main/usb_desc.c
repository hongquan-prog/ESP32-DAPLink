#include "usb_desc.h"
#include "tusb_cdc_acm.h"

#define TUSB_DESC_WITH_MSC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_INOUT_DESC_LEN)
#define TUSB_DESC_WITHOUT_MSC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_INOUT_DESC_LEN)

static uint8_t const desc_hid_dap_report[] = {
    TUD_HID_REPORT_DESC_GENERIC_INOUT(CFG_TUD_HID_EP_BUFSIZE)};

static uint8_t const desc_configuration_with_msc[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_WITH_MSC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, STRID_CDC_INTERFACE, EDPT_CDC_NOTIFY, 8, EDPT_CDC_OUT, EDPT_CDC_IN, CFG_TUD_CDC_EP_BUFSIZE),
    // Interface number, string index, protocol, report descriptor len, EP In & Out address, size & polling interval
    TUD_HID_INOUT_DESCRIPTOR(ITF_NUM_HID, STRID_HID_INTERFACE, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_dap_report), EDPT_HID_OUT, EDPT_HID_IN, CFG_TUD_HID_EP_BUFSIZE, 1),
    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, STRID_MSC_INTERFACE, EDPT_MSC_OUT, EDPT_MSC_IN, TUD_OPT_HIGH_SPEED ? 512 : 64)};

static char const *string_desc_arr_with_msc[] = {
    (const char[]){0x09, 0x04},              // 0: is supported language is English (0x0409)
    CONFIG_TINYUSB_DESC_MANUFACTURER_STRING, // 1: Manufacturer
    CONFIG_TINYUSB_DESC_PRODUCT_STRING,      // 2:  The value of this macro _must_ include the string "CMSIS-DAP". Otherwise debuggers will not recognizethe USB device
    CONFIG_TINYUSB_DESC_SERIAL_STRING,       // 3. SN
    CONFIG_TINYUSB_DESC_CDC_STRING,          // 4. CDC
    CONFIG_TINYUSB_DESC_HID_STRING,          // 5. HID
    CONFIG_TINYUSB_DESC_MSC_STRING           // 6. MSC
};

static uint8_t const desc_configuration_without_msc[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_WITHOUT_MSC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, STRID_CDC_INTERFACE, EDPT_CDC_NOTIFY, 8, EDPT_CDC_OUT, EDPT_CDC_IN, CFG_TUD_CDC_EP_BUFSIZE),
    // Interface number, string index, protocol, report descriptor len, EP In & Out address, size & polling interval
    TUD_HID_INOUT_DESCRIPTOR(ITF_NUM_HID, STRID_HID_INTERFACE, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_dap_report), EDPT_HID_OUT, EDPT_HID_IN, CFG_TUD_HID_EP_BUFSIZE, 1)};

static char const *string_desc_arr_without_msc[] = {
    (const char[]){0x09, 0x04},              // 0: is supported language is English (0x0409)
    CONFIG_TINYUSB_DESC_MANUFACTURER_STRING, // 1: Manufacturer
    CONFIG_TINYUSB_DESC_PRODUCT_STRING,      // 2:  The value of this macro _must_ include the string "CMSIS-DAP". Otherwise debuggers will not recognizethe USB device
    CONFIG_TINYUSB_DESC_SERIAL_STRING,       // 3. SN
    CONFIG_TINYUSB_DESC_CDC_STRING,          // 4. CDC
    CONFIG_TINYUSB_DESC_HID_STRING,          // 5. HID
};

static tusb_desc_device_t descriptor_config = {
    .bLength = sizeof(descriptor_config),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0100,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = CONFIG_TINYUSB_DESC_CUSTOM_VID,
    .idProduct = CONFIG_TINYUSB_DESC_CUSTOM_PID,
    .bcdDevice = CONFIG_TINYUSB_DESC_BCD_DEVICE,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    return desc_hid_dap_report;
}

tusb_desc_device_t *get_device_descriptor(void)
{
    return &descriptor_config;
}

const char **get_string_descriptor(bool with_msc)
{
    return with_msc ? string_desc_arr_with_msc : string_desc_arr_without_msc;
}

int get_string_descriptor_count(bool with_msc)
{
    return with_msc ? (sizeof(string_desc_arr_with_msc) / sizeof(char *)) : (sizeof(string_desc_arr_without_msc) / sizeof(char *));
}

const uint8_t *get_configuration_descriptor(bool with_msc)
{
    return with_msc ? desc_configuration_with_msc : desc_configuration_without_msc;
}