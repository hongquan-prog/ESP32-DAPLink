/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   add license declaration
 * 2026-3-17     refactor     Add deinit/suspend/resume functions
 */

#include "serial/cdc_uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdatomic.h>

typedef struct
{
    uart_port_t uart;
    cdc_uart_cb_t cb[CDC_UART_HANDLER_NUM];
    TaskHandle_t task_handle;
    atomic_bool suspended;
    atomic_bool initialized;
} cdc_uart_t;

static cdc_uart_t s_cdc_uart = {0};
static const char *TAG = "cdc_uart";
static void cdc_uart_rx_task(void *param);

bool cdc_uart_init(uart_port_t uart, gpio_num_t tx_pin, gpio_num_t rx_pin, int baudrate)
{
    bool ret = false;
    uart_config_t uart_config =
    {
        .baud_rate = baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    /* Check if already initialized */
    if (atomic_load(&s_cdc_uart.initialized))
    {
        ESP_LOGW(TAG, "cdc_uart already initialized");
        return true;
    }

    s_cdc_uart.uart = uart;
    ret = (ESP_OK == uart_driver_install(s_cdc_uart.uart, 2 * 1024, 2 * 1024, 0, NULL, 0));
    ret = ret && (ESP_OK == uart_param_config(s_cdc_uart.uart, &uart_config));
    ret = ret && (ESP_OK == uart_set_pin(s_cdc_uart.uart, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    if (ret)
    {
        atomic_store(&s_cdc_uart.suspended, false);
        xTaskCreate(cdc_uart_rx_task, "cdc_uart_rx_task", 4096, (void *)&s_cdc_uart, 10, &s_cdc_uart.task_handle);
        atomic_store(&s_cdc_uart.initialized, true);
    }

    return ret;
}

bool cdc_uart_deinit(void)
{
    if (!atomic_load(&s_cdc_uart.initialized))
    {
        ESP_LOGW(TAG, "cdc_uart not initialized");
        return true;
    }

    // Delete the task
    if (s_cdc_uart.task_handle)
    {
        vTaskDelete(s_cdc_uart.task_handle);
        s_cdc_uart.task_handle = NULL;
    }

    // Delete UART driver
    uart_driver_delete(s_cdc_uart.uart);

    atomic_store(&s_cdc_uart.initialized, false);
    atomic_store(&s_cdc_uart.suspended, false);

    ESP_LOGI(TAG, "cdc_uart deinitialized");
    return true;
}

bool cdc_uart_suspend(void)
{
    if (!atomic_load(&s_cdc_uart.initialized))
    {
        return false;
    }

    if (!atomic_load(&s_cdc_uart.suspended))
    {
        atomic_store(&s_cdc_uart.suspended, true);
        if (s_cdc_uart.task_handle)
        {
            vTaskSuspend(s_cdc_uart.task_handle);
        }
        ESP_LOGI(TAG, "cdc_uart suspended");
    }

    return true;
}

bool cdc_uart_resume(void)
{
    if (!atomic_load(&s_cdc_uart.initialized))
    {
        return false;
    }

    if (atomic_load(&s_cdc_uart.suspended))
    {
        atomic_store(&s_cdc_uart.suspended, false);
        if (s_cdc_uart.task_handle)
        {
            vTaskResume(s_cdc_uart.task_handle);
        }
        ESP_LOGI(TAG, "cdc_uart resumed");
    }

    return true;
}

bool cdc_uart_set_baudrate(uint32_t baudrate)
{
    return (ESP_OK == uart_set_baudrate(s_cdc_uart.uart, baudrate));
}

bool cdc_uart_get_baudrate(uint32_t *baudrate)
{
    return (ESP_OK == uart_get_baudrate(s_cdc_uart.uart, baudrate));
}

bool cdc_uart_set_config(int data_bits, int parity, int stop_bits)
{
    uart_word_length_t data_bits_val = UART_DATA_8_BITS;
    uart_parity_t parity_val = UART_PARITY_DISABLE;
    uart_stop_bits_t stop_bits_val = UART_STOP_BITS_1;
    esp_err_t ret = ESP_OK;

    /* Convert data bits */
    switch (data_bits)
    {
    case 5:
        data_bits_val = UART_DATA_5_BITS;
        break;
    case 6:
        data_bits_val = UART_DATA_6_BITS;
        break;
    case 7:
        data_bits_val = UART_DATA_7_BITS;
        break;
    case 8:
    default:
        data_bits_val = UART_DATA_8_BITS;
        break;
    }

    /* Convert parity: 0=none, 1=odd, 2=even */
    switch (parity)
    {
    case 1:
        parity_val = UART_PARITY_ODD;
        break;
    case 2:
        parity_val = UART_PARITY_EVEN;
        break;
    case 0:
    default:
        parity_val = UART_PARITY_DISABLE;
        break;
    }

    /* Convert stop bits: 1=1bit, 2=2bits */
    switch (stop_bits)
    {
    case 2:
        stop_bits_val = UART_STOP_BITS_2;
        break;
    case 1:
    default:
        stop_bits_val = UART_STOP_BITS_1;
        break;
    }

    ret = uart_set_word_length(s_cdc_uart.uart, data_bits_val);
    if (ret != ESP_OK)
    {
        return false;
    }

    ret = uart_set_parity(s_cdc_uart.uart, parity_val);
    if (ret != ESP_OK)
    {
        return false;
    }

    ret = uart_set_stop_bits(s_cdc_uart.uart, stop_bits_val);
    if (ret != ESP_OK)
    {
        return false;
    }

    ESP_LOGI(TAG, "UART config set: data_bits=%d, parity=%d, stop_bits=%d", data_bits, parity, stop_bits);

    return true;
}

bool cdc_uart_write(const void *src, size_t size)
{
    return (0 <= uart_write_bytes(s_cdc_uart.uart, src, size));
}

void cdc_uart_register_rx_handler(cdc_uart_handler_def handler, cdc_uart_rx_callback_t func, void *context)
{
    if ((handler >= CDC_UART_HANDLER_NUM) || !func)
    {
        return;
    }

    s_cdc_uart.cb[handler].func = func;
    s_cdc_uart.cb[handler].usr_data = context;
}

static void cdc_uart_rx_task(void *param)
{
#define RX_BUF_SIZE 64

    int offset = 0;
    int need = 0;
    int read = 0;
    uint8_t data[RX_BUF_SIZE] = {0};
    cdc_uart_t *cdc_uart = (cdc_uart_t *)param;

    for (;;)
    {
        need = RX_BUF_SIZE - offset;
        read = uart_read_bytes(cdc_uart->uart, data + offset, need, pdMS_TO_TICKS(5));

        if (read < 0)
        {
            ESP_LOGE(TAG, "uart_read_bytes() failed: %d", read);
        }
        else
        {
            offset += read;

            if ((offset == RX_BUF_SIZE) || ((read == 0) && (offset > 0)))
            {
                for (int i = 0; i < CDC_UART_HANDLER_NUM; i++)
                {
                    if (s_cdc_uart.cb[i].func)
                    {
                        s_cdc_uart.cb[i].func(s_cdc_uart.cb[i].usr_data, data, offset);
                    }
                }

                offset = 0;
            }
        }
    }
}