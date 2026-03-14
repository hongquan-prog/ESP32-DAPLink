/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"

#if CONFIG_DEBUG_PROBE_IFACE_SWD

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_log.h"
#include "debug_probe.h"
#include "DAP_config.h"
#include "DAP.h"

static const char *TAG = "eub_swd";

#define SWD_RCVBUF_SIZE     1024
#define SWD_SNDBUF_SIZE     1024
static RingbufHandle_t s_swd_rcvbuf = NULL;
static RingbufHandle_t s_swd_sndbuf = NULL;

static TaskHandle_t s_swd_task_handle = NULL;

esp_err_t debug_probe_init(void) __attribute__((alias("swd_init")));
esp_err_t debug_probe_process_data(const uint8_t *data, size_t len) __attribute__((alias("swd_process_data")));
int debug_probe_get_proto_caps(void *dest) __attribute__((alias("swd_get_proto_caps")));
void debug_probe_handle_esp32_tdi_bootstrapping(bool rebooting) __attribute__((alias("swd_handle_esp32_tdi_bootstrapping")));
uint8_t *debug_probe_get_data_to_send(size_t *len, TickType_t timeout) __attribute__((alias("swd_get_data_to_send")));
void debug_probe_free_sent_data(uint8_t *data) __attribute__((alias("swd_free_sent_data")));
debug_probe_cmd_response_t debug_probe_handle_command(uint8_t cmd, uint16_t wValue) __attribute__((alias("swd_handle_command")));

static int swd_get_proto_caps(void *dest)
{
    return 0;
}

static void swd_handle_esp32_tdi_bootstrapping(bool rebooting)
{
    // Nothing to do for SWD
}

// Process data received from transport (USB/UART/etc)
static esp_err_t swd_process_data(const uint8_t *data, size_t len)
{
    BaseType_t res = xRingbufferSend(s_swd_rcvbuf, data, len, pdMS_TO_TICKS(1000));
    if (res != pdTRUE) {
        ESP_LOGE(TAG, "Cannot write to SWD receive buffer (free %zu of %d)!",
                 xRingbufferGetCurFreeSize(s_swd_rcvbuf), SWD_RCVBUF_SIZE);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static uint8_t *swd_get_data_to_send(size_t *len, TickType_t timeout)
{
    uint8_t *data = xRingbufferReceive(s_swd_sndbuf, len, timeout);
    if (data && *len > 0) {
        ESP_LOG_BUFFER_HEXDUMP(TAG, data, *len, ESP_LOG_DEBUG);
    }
    return data;
}

static void swd_free_sent_data(uint8_t *data)
{
    vRingbufferReturnItem(s_swd_sndbuf, data);
}

static esp_err_t swd_send_data(const void *buf, const size_t size)
{
    if (xRingbufferSend(s_swd_sndbuf, buf, size, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Cannot write to SWD send buffer (free %zu of %d)!",
                 xRingbufferGetCurFreeSize(s_swd_sndbuf), SWD_SNDBUF_SIZE);
        return ESP_FAIL;
    }
    return ESP_OK;
}

debug_probe_cmd_response_t swd_handle_command(uint8_t cmd, uint16_t wValue)
{
    // Function to handle the TUSB_REQ_TYPE_VENDOR control requests from the host,
    // called by the tusb_control_xfer_cb function in eub_vendord.c.
    debug_probe_cmd_response_t response = {0};
    return response;
}

static void swd_task(void *pvParameters)
{
    size_t total_bytes = 0;
    uint8_t response[DAP_PACKET_SIZE];

    s_swd_task_handle = xTaskGetCurrentTaskHandle();

    while (1) {
        uint8_t *request = xRingbufferReceive(s_swd_rcvbuf, &total_bytes, portMAX_DELAY);

        ESP_LOG_BUFFER_HEXDUMP("req", request, total_bytes, ESP_LOG_DEBUG);

        uint32_t resp_len = DAP_ProcessCommand(request, response) & 0xFFFF; //lower 16 bits are response len

        ESP_LOG_BUFFER_HEXDUMP("res", response, resp_len, ESP_LOG_DEBUG);

        swd_send_data(response, resp_len);

        vRingbufferReturnItem(s_swd_rcvbuf, request);
    }

    vTaskDelete(NULL);
}

static esp_err_t swd_init(void)
{
    ESP_LOGI(TAG, "Initializing SWD debug probe");

    s_swd_rcvbuf = xRingbufferCreate(SWD_RCVBUF_SIZE, RINGBUF_TYPE_NOSPLIT);
    s_swd_sndbuf = xRingbufferCreate(SWD_SNDBUF_SIZE, RINGBUF_TYPE_NOSPLIT);
    if (!s_swd_rcvbuf || !s_swd_sndbuf) {
        ESP_LOGE(TAG, "Cannot allocate SWD buffers!");
        return ESP_FAIL;
    }

    // Compile-time verification of packet size compatibility
    _Static_assert(DEBUG_PROBE_PACKET_SIZE == DAP_PACKET_SIZE,
                   "DEBUG_PROBE_PACKET_SIZE must equal DAP_PACKET_SIZE");

    DAP_Setup();

    BaseType_t res = xTaskCreatePinnedToCore(swd_task,
                     "swd_task",
                     4 * 1024,
                     NULL,
                     DEBUG_PROBE_TASK_PRI,
                     &s_swd_task_handle,
                     esp_cpu_get_core_id());
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Cannot create SWD task!");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

#endif /* CONFIG_DEBUG_PROBE_IFACE_SWD */
