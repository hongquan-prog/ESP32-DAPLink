/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include "esp_log.h"
#include <errno.h>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "sdkconfig.h"
#include "cdc_uart.h"
#include "cdc_handle.h"
#include "tusb_config.h"
#include "DAP_config.h"
#include "DAP.h"
#include "main.h"
#include "msc_disk.h"
#include "hex_prog.h"
#include "algo_extractor.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "protocol_examples_common.h"

static const char *TAG = "main";

static uint8_t const desc_hid_dap_report[] = {
    TUD_HID_REPORT_DESC_GENERIC_INOUT(CFG_TUD_HID_EP_BUFSIZE)};

static uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, STRID_MSC_INTERFACE, EDPT_MSC_OUT, EDPT_MSC_IN, TUD_OPT_HIGH_SPEED ? 512 : 64),
    // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, STRID_CDC_INTERFACE, EDPT_CDC_NOTIFY, 8, EDPT_CDC_OUT, EDPT_CDC_IN, CFG_TUD_CDC_EP_BUFSIZE),
    // Interface number, string index, protocol, report descriptor len, EP In & Out address, size & polling interval
    TUD_HID_INOUT_DESCRIPTOR(ITF_NUM_HID, STRID_HID_INTERFACE, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_dap_report), EDPT_HID_OUT, EDPT_HID_IN, CFG_TUD_HID_EP_BUFSIZE, 1)};

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

static char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04},              // 0: is supported language is English (0x0409)
    CONFIG_TINYUSB_DESC_MANUFACTURER_STRING, // 1: Manufacturer
    CONFIG_TINYUSB_DESC_PRODUCT_STRING,      // 2:  The value of this macro _must_ include the string "CMSIS-DAP". Otherwise debuggers will not recognizethe USB device
    CONFIG_TINYUSB_DESC_SERIAL_STRING,       // 3. SN
    CONFIG_TINYUSB_DESC_CDC_STRING,          // 4. CDC
    CONFIG_TINYUSB_DESC_MSC_STRING,          // 5. MSC
    CONFIG_TINYUSB_DESC_HID_STRING,          // 6. HID
};

static httpd_handle_t http_server = NULL;

static esp_err_t webserial_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET)
    {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }

    return false;
}

static esp_err_t index_handler(httpd_req_t *req)
{
#define HTTPD_HDR_STR "HTTP/1.1 200 OK\r\n"         \
                      "Content-Type: text/html\r\n" \
                      "Content-Length: %d\r\n"
#define BUFFER_SIZE 512

    FILE *fp = nullptr;
    size_t file_size = 0;
    size_t rd_size = 0;
    char *buf = nullptr;
    esp_err_t ret = ESP_FAIL;

    ESP_LOGI(TAG, "%d connected", httpd_req_to_sockfd(req));

    try
    {
        if (req->method == HTTP_GET)
        {
            buf = new char[BUFFER_SIZE];
            fp = fopen("/data/index.html", "r");

            if (!fp)
                throw std::ifstream::failure("/data/index.html not exist");

            fseek(fp, 0, SEEK_END);
            file_size = ftell(fp);
            rd_size = snprintf(buf, BUFFER_SIZE, HTTPD_HDR_STR, file_size);
            httpd_socket_send(req->handle, httpd_req_to_sockfd(req), buf, rd_size, 0);
            fseek(fp, 0, SEEK_SET);

            while (feof(fp) != EOF)
            {
                rd_size = fread(buf, 1, BUFFER_SIZE, fp);
                httpd_socket_send(req->handle, httpd_req_to_sockfd(req), buf, rd_size, 0);
            }

            ret = ESP_OK;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }

    if (fp)
        fclose(fp);

    if (buf)
        delete[] buf;

    return ret;
}

static const httpd_uri_t webserial = {
    .uri = "/webserial",
    .method = HTTP_GET,
    .handler = webserial_handler,
    .user_ctx = NULL,
    .is_websocket = true,
    .handle_ws_control_frames = false,
    .supported_subprotocol = nullptr};

static const httpd_uri_t index_html = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = nullptr};

void web_uart_rx_callback(int uart, void *usr_data, uint8_t *data, size_t size)
{
    static httpd_ws_frame_t ws_pkt = {false, false, HTTPD_WS_TYPE_TEXT, data, size};
    size_t clients = 2;
    int client_fds[2];

    if (httpd_get_client_list(http_server, &clients, client_fds) == ESP_OK)
    {
        for (size_t i = 0; i < clients; ++i)
        {
            ESP_LOGI(TAG, "%d", httpd_ws_get_fd_info(http_server, client_fds[i]));
            if (httpd_ws_get_fd_info(http_server, client_fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET)
            {
                ESP_LOGI(TAG, "Active client (fd=%d) -> sending async message", client_fds[i]);
                httpd_ws_send_frame_async(http_server, client_fds[i], &ws_pkt);
            }
        }
    }    
}

extern "C" uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return desc_hid_dap_report;
}

extern "C" uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    // TODO not Implemented
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

extern "C" void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    static uint8_t s_tx_buf[CFG_TUD_HID_EP_BUFSIZE];
    // This doesn't use multiple report and report ID
    (void)instance;
    (void)report_id;
    (void)report_type;

    DAP_ProcessCommand(buffer, s_tx_buf);
    tud_hid_report(0, s_tx_buf, sizeof(s_tx_buf));
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Registering the ws handler
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &webserial);
        httpd_register_uri_handler(server, &index_html);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void disconnect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server)
    {
        ESP_LOGI(TAG, "Stopping webserver");
        if (httpd_stop(*server) == ESP_OK)
        {
            *server = NULL;
        }
        else
        {
            ESP_LOGE(TAG, "Failed to stop http server");
        }
    }
}

static void connect_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;

    if (*server == NULL)
    {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

extern "C" void app_main(void)
{
    // TickType_t start_time = 0;
    // static HexProg programmer;
    // static AlgoExtractor extractor;
    // static FlashIface::program_target_t target;
    // static FlashIface::target_cfg_t cfg;

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &http_server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &http_server));

    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &descriptor_config,
        .string_descriptor = string_desc_arr,
        .string_descriptor_count = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]),
        .external_phy = false,
        .configuration_descriptor = desc_configuration,
        .self_powered = false,
        .vbus_monitor_io = 0};

    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = 64,
        .callback_rx = cdc_rx_callback, // the first way to register a callback
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = cdc_line_codinig_changed_callback};

    //static const cdc_uart_cb_t usb_uart_cb = {.func = cdc_uart_rx_callback, .usr_data =  (void *)TINYUSB_CDC_ACM_0};
    static const cdc_uart_cb_t web_uart_cb = {.func = web_uart_rx_callback, .usr_data = (void *)TINYUSB_CDC_ACM_0};

    DAP_Setup();
    ESP_LOGI(TAG, "USB initialization");
    msc_dick_mount(CONFIG_TINYUSB_MSC_MOUNT_PATH);
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
    cdc_uart_init(UART_NUM_1, GPIO_NUM_13, GPIO_NUM_14, 115200);
    cdc_uart_register_rx_callback(&web_uart_cb);
    ESP_LOGI(TAG, "USB initialization DONE");

    http_server = start_webserver();
    //     extractor.extract("/data/algo/GD/F4/GD32F4xx_1MB.FLM", target, cfg);
    //     printf("Device Name and Description: %s\n", cfg.device_name.c_str());
    //     for (int i = 0; i < cfg.sector_info.size(); i++)
    //     {
    //         int sec_num = (i != (cfg.sector_info.size() - 1)) ? ((cfg.sector_info[i + 1].start - cfg.sector_info[i].start) / cfg.sector_info[i].size) : ((cfg.flash_regions[0].end - cfg.sector_info[i].start) / cfg.sector_info[i].size);
    //         printf("Sector info %d  start addr: 0x%lx, secrot size: 0x%lx * %d\n", i, cfg.sector_info[i].start, cfg.sector_info[i].size, sec_num);
    //     }

    //     for (int i = 0; i < cfg.flash_regions.size(); i++)
    //     {
    //         if (cfg.flash_regions[i].start == 0 && cfg.flash_regions[i].end == 0)
    //             break;

    //         printf("Flash region %d addr: 0x%lx, end: 0x%lx\n", i, cfg.flash_regions[i].start, cfg.flash_regions[i].end);
    //     }

    //     for (int i = 0; i < cfg.ram_regions.size(); i++)
    //     {
    //         if (cfg.ram_regions[i].start == 0 && cfg.ram_regions[i].end == 0)
    //             break;

    //         printf("RAM region %d addr: 0x%lx, end: 0x%lx\n", i, cfg.ram_regions[i].start, cfg.ram_regions[i].end);
    //     }

    //     while (1)
    //     {
    //         start_time = xTaskGetTickCount();
    //         programmer.programing_hex(cfg, "/data/GPIO-LED.hex");
    //         ESP_LOGI(TAG, "Elapsed time %ld ms", (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS);
    //         vTaskDelay(5000);
    //     }

    //     if (target.algo_blob)
    //         delete[] target.algo_blob;
}
