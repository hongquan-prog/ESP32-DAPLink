#include "cdc_handle.h"
#include "cdc_uart.h"
#include "esp_log.h"

static const char *TAG = "cdc_handle";

void cdc_line_codinig_changed_callback(int itf, cdcacm_event_t *event)
{
    cdc_line_coding_t const *coding = event->line_coding_changed_data.p_line_coding;
    uint32_t baudrate = 0;

    if (cdc_uart_get_baudrate(&baudrate) && (baudrate != coding->bit_rate))
    {
        cdc_uart_set_baudrate(coding->bit_rate);
    }
}

void cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    static uint8_t buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];
    size_t rx_size = 0;
    esp_err_t ret = tinyusb_cdcacm_read(itf, buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);

    if (ret == ESP_OK)
    {
        cdc_uart_write(buf, rx_size);
    }
}

void cdc_uart_rx_callback(int uart, void *usr_data, uint8_t *data, size_t size)
{
    ESP_LOGD(TAG, "uart %d, data %p, size %d", uart, data, size);

    if (tud_cdc_n_connected((int)usr_data))
    {
        tinyusb_cdcacm_write_queue((tinyusb_cdcacm_itf_t)usr_data, data, size);
        tinyusb_cdcacm_write_flush((tinyusb_cdcacm_itf_t)usr_data, 1);
    }
}