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

#include "driver/uart.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*cdc_uart_rx_callback_t)(void *usr_data, uint8_t *data, size_t size);

typedef struct
{
    cdc_uart_rx_callback_t func;
    void *usr_data;
} cdc_uart_cb_t;

typedef enum
{
    CDC_UART_USB_HANDLER,
    CDC_UART_WEB_HANDLER,
    CDC_UART_HANDLER_NUM
} cdc_uart_handler_def;

/**
 * @brief Initialize the CDC UART interface
 * @param uart UART port number
 * @param tx_pin TX GPIO pin number
 * @param rx_pin RX GPIO pin number
 * @param buadrate Baud rate setting
 * @return true on success, false on failure
 */
bool cdc_uart_init(uart_port_t uart, gpio_num_t tx_pin, gpio_num_t rx_pin, int buadrate);

/**
 * @brief Deinitialize the CDC UART interface
 * @return true on success, false on failure
 */
bool cdc_uart_deinit(void);

/**
 * @brief Suspend the CDC UART interface
 * @return true on success, false on failure
 */
bool cdc_uart_suspend(void);

/**
 * @brief Resume the CDC UART interface
 * @return true on success, false on failure
 */
bool cdc_uart_resume(void);

/**
 * @brief Set the UART baud rate
 * @param baudrate New baud rate value
 * @return true on success, false on failure
 */
bool cdc_uart_set_baudrate(uint32_t baudrate);

/**
 * @brief Get the current UART baud rate
 * @param baudrate Pointer to store the baud rate value
 * @return true on success, false on failure
 */
bool cdc_uart_get_baudrate(uint32_t *baudrate);

/**
 * @brief Set UART configuration (data bits, parity, stop bits)
 * @param data_bits Number of data bits (5-8)
 * @param parity Parity mode (0=none, 1=odd, 2=even)
 * @param stop_bits Number of stop bits (1-2)
 * @return true on success, false on failure
 */
bool cdc_uart_set_config(int data_bits, int parity, int stop_bits);

/**
 * @brief Write data to UART
 * @param src Pointer to data buffer to send
 * @param size Size of data to send
 * @return true on success, false on failure
 */
bool cdc_uart_write(const void *src, size_t size);

/**
 * @brief Register a receive callback handler
 * @param handler Handler type (USB or WEB)
 * @param func Callback function pointer
 * @param context User context passed to callback
 */
void cdc_uart_register_rx_handler(cdc_uart_handler_def handler, cdc_uart_rx_callback_t func, void *context);

#ifdef __cplusplus
}
#endif