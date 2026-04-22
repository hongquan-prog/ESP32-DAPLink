/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-3-17      refactor     Initial version for web serial refactoring
 */

#include <inttypes.h>
#include <string.h>
#include "serial/serial_manager.h"
#include "serial/cdc_uart.h"
#if defined(CONFIG_USB_DEBUG_PROBE)
#include "usb/usb_cdc_handler.h"
#endif
#include "web/web_handler.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "serial_manager";

/**
 * @brief Serial manager context structure (opaque implementation)
 */
typedef struct
{
    SemaphoreHandle_t mutex;                    ///< Mutex for state switching
    volatile serial_state_t state;              ///< Current state
    volatile int web_client_count;              ///< Web client count
    volatile bool usb_connected;                ///< USB connection status
    volatile bool uart_initialized;             ///< UART initialization status
    uart_port_t uart_num;                       ///< UART port number
    gpio_num_t tx_pin;                          ///< TX GPIO pin
    gpio_num_t rx_pin;                          ///< RX GPIO pin
    int baudrate;                               ///< Current baudrate
} serial_manager_ctx_t;

/* Singleton instance */
static serial_manager_ctx_t s_ctx =
{
    .mutex = NULL,
    .state = SERIAL_STATE_IDLE,
    .web_client_count = 0,
    .usb_connected = false,
    .uart_initialized = false,
    .uart_num = UART_NUM_1,
    .tx_pin = GPIO_NUM_13,
    .rx_pin = GPIO_NUM_14,
    .baudrate = 115200
};

static void _switch_state(serial_state_t new_state);
static void _update(void);

bool serial_manager_init(uart_port_t uart, gpio_num_t tx_pin, gpio_num_t rx_pin, int baudrate)
{
    if (s_ctx.mutex == NULL)
    {
        s_ctx.mutex = xSemaphoreCreateMutex();
        if (s_ctx.mutex == NULL)
        {
            ESP_LOGE(TAG, "Failed to create mutex");
            return false;
        }
    }

    if (xSemaphoreTake(s_ctx.mutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return false;
    }

    s_ctx.uart_num = uart;
    s_ctx.tx_pin = tx_pin;
    s_ctx.rx_pin = rx_pin;
    s_ctx.baudrate = baudrate;

    xSemaphoreGive(s_ctx.mutex);

    ESP_LOGI(TAG, "serial_manager initialized: UART%d, TX=%d, RX=%d, baud=%d",
             uart, tx_pin, rx_pin, baudrate);

    return true;
}

serial_state_t serial_manager_get_state(void)
{
    return s_ctx.state;
}

static void _update(void)
{
    serial_state_t current = s_ctx.state;
    bool usb = s_ctx.usb_connected;
    int web_count = s_ctx.web_client_count;

    serial_state_t new_state = current;

    /* Priority: STATE_USB > STATE_WEB > STATE_IDLE */
    if (usb)
    {
        new_state = SERIAL_STATE_USB;
    }
    else if (web_count > 0)
    {
        new_state = SERIAL_STATE_WEB;
    }
    else
    {
        new_state = SERIAL_STATE_IDLE;
    }

    if (new_state != current)
    {
        _switch_state(new_state);
    }
}

static void _switch_state(serial_state_t new_state)
{
    serial_state_t old_state = s_ctx.state;

    ESP_LOGI(TAG, "State switching: %d -> %d", old_state, new_state);

    s_ctx.state = new_state;

    /* Notify web clients about state change */
    web_notify_serial_state((int)new_state);
}

void serial_manager_web_client_connected(void)
{
    if (s_ctx.mutex && xSemaphoreTake(s_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        s_ctx.web_client_count++;
        ESP_LOGI(TAG, "Web client connected, count: %d", s_ctx.web_client_count);
        xSemaphoreGive(s_ctx.mutex);
    }
    else
    {
        s_ctx.web_client_count++;
        ESP_LOGI(TAG, "Web client connected, count: %d", s_ctx.web_client_count);
    }
    _update();
}

void serial_manager_web_client_disconnected(void)
{
    if (s_ctx.mutex && xSemaphoreTake(s_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        if (s_ctx.web_client_count > 0)
        {
            s_ctx.web_client_count--;
        }
        ESP_LOGI(TAG, "Web client disconnected, count: %d", s_ctx.web_client_count);
        xSemaphoreGive(s_ctx.mutex);
    }
    else
    {
        if (s_ctx.web_client_count > 0)
        {
            s_ctx.web_client_count--;
        }
        ESP_LOGI(TAG, "Web client disconnected, count: %d", s_ctx.web_client_count);
    }
    _update();
}

int serial_manager_get_web_client_count(void)
{
    return s_ctx.web_client_count;
}

void serial_manager_set_usb_connected(bool connected)
{
    bool was_connected = s_ctx.usb_connected;
    s_ctx.usb_connected = connected;

    if (was_connected != connected)
    {
        ESP_LOGI(TAG, "USB connection status changed: %s", connected ? "connected" : "disconnected");
        _update();
    }
}

bool serial_manager_is_usb_connected(void)
{
    return s_ctx.usb_connected;
}

void serial_manager_handle_uart_data(const uint8_t *data, size_t len)
{
    serial_state_t current = s_ctx.state;

    switch (current)
    {
    case SERIAL_STATE_USB:
#if defined(CONFIG_USB_DEBUG_PROBE)
        usb_cdc_send_to_host((void *)TINYUSB_CDC_ACM_0, (uint8_t *)data, len);
#endif
        break;

    case SERIAL_STATE_WEB:
        /* Web clients receive data via web_send_to_clients callback */
        /* This is handled by the cdc_uart callback mechanism */
        break;

    case SERIAL_STATE_IDLE:
    default:
        /* Discard data in idle state */
        break;
    }
}

bool serial_manager_send_to_uart(const uint8_t *data, size_t len)
{
    serial_state_t current = s_ctx.state;

    if (current == SERIAL_STATE_IDLE)
    {
        return false;  /* Don't send in idle state */
    }

    return cdc_uart_write(data, len);
}

bool serial_manager_set_baudrate(uint32_t baudrate)
{
    if (s_ctx.mutex && xSemaphoreTake(s_ctx.mutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return false;
    }

    bool success = cdc_uart_set_baudrate(baudrate);
    if (success)
    {
        s_ctx.baudrate = baudrate;
        ESP_LOGI(TAG, "Baudrate set to %" PRIu32, baudrate);
    }

    if (s_ctx.mutex)
    {
        xSemaphoreGive(s_ctx.mutex);
    }

    return success;
}

bool serial_manager_get_baudrate(uint32_t *baudrate)
{
    if (baudrate)
    {
        *baudrate = (uint32_t)s_ctx.baudrate;
        return true;
    }
    return false;
}

uart_port_t serial_manager_get_uart_port(void)
{
    return s_ctx.uart_num;
}

bool serial_manager_set_config(int data_bits, int parity, int stop_bits)
{
    return cdc_uart_set_config(data_bits, parity, stop_bits);
}