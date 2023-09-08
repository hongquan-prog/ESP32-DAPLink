/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   add license declaration
 */
#pragma once

#include "cdc_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    UART_USB_HANDLER,
    UART_WEB_HANDLER,
    UART_HANDLER_NUM
} uart_handler_def;

void uart_rx_handler_init(uart_handler_def handler, cdc_uart_rx_callback_t callback, void *context);
void uart_rx_handler_switch(uart_handler_def handler);

#ifdef __cplusplus
}
#endif