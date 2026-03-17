/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   add license declaration
 * 2026-3-17     refactor     Add USB connection state detection
 */
#include "usb_cdc_handler.h"
#include "cdc_uart.h"
#include "esp_log.h"
#include "serial_manager.h"

#define TAG "usb_cdc_handler"

void usb_cdc_send_to_host(void *context, uint8_t *data, size_t size)
{
    ESP_LOGD(TAG, "data %p, size %d", data, size);

    /* Only send to USB when in USB state */
    if (serial_manager_get_state() != SERIAL_STATE_USB)
    {
        return;
    }

    if (tud_cdc_n_connected((int)context))
    {
        tinyusb_cdcacm_write_queue((tinyusb_cdcacm_itf_t)context, data, size);
        tinyusb_cdcacm_write_flush((tinyusb_cdcacm_itf_t)context, 1);
    }
}

void usb_cdc_set_line_codinig(int itf, cdcacm_event_t *event)
{
    cdc_line_coding_t const *coding = event->line_coding_changed_data.p_line_coding;
    uint32_t baudrate = 0;

    if (cdc_uart_get_baudrate(&baudrate) && (baudrate != coding->bit_rate))
    {
        cdc_uart_set_baudrate(coding->bit_rate);
    }
}

void usb_cdc_send_to_uart(int itf, cdcacm_event_t *event)
{
    static uint8_t buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];
    size_t rx_size = 0;
    esp_err_t ret = tinyusb_cdcacm_read(itf, buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);

    if (ret == ESP_OK)
    {
        cdc_uart_write(buf, rx_size);
    }
}

void usb_cdc_line_state_changed(int itf, cdcacm_event_t *event)
{
    /*
     * DTR is asserted when a terminal application connects.
     * We use DTR to detect USB CDC connection.
     */
    bool dtr = event->line_state_changed_data.dtr;
    bool rts = event->line_state_changed_data.rts;

    ESP_LOGI(TAG, "USB CDC line state changed: DTR=%d, RTS=%d", dtr, rts);

    /* Notify SerialManager about USB connection state */
    serial_manager_set_usb_connected(dtr);
}