/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   add license declaration
 */
#include "uart_handler.h"
#include "esp_log.h"

#define TAG "uart_handler"

static cdc_uart_cb_t s_handler[UART_HANDLER_NUM] = {0};

void uart_rx_handler_init(uart_handler_def handler, cdc_uart_rx_callback_t callback, void *context)
{
    if ((handler >= UART_HANDLER_NUM) || (cdc_uart_get_rx_callback() == &s_handler[handler]))
    {   
        return;
    }

    s_handler[handler].func = callback;
    s_handler[handler].usr_data = context;
}

void uart_rx_handler_switch(uart_handler_def handler)
{
    if ((handler >= UART_HANDLER_NUM) || (cdc_uart_get_rx_callback() == &s_handler[handler]))
    {
        return;
    }

    cdc_uart_register_rx_callback(&s_handler[handler]);
}