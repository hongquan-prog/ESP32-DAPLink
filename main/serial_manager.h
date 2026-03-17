/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-3-17      refactor     Initial version for web serial refactoring
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "driver/uart.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Serial port state enumeration
 */
typedef enum
{
    SERIAL_STATE_IDLE,     ///< Idle state, UART closed
    SERIAL_STATE_USB,      ///< USB CDC connected
    SERIAL_STATE_WEB       ///< Web client connected
} serial_state_t;

/**
 * @brief Initialize serial manager
 * @param uart UART port number
 * @param tx_pin TX GPIO pin
 * @param rx_pin RX GPIO pin
 * @param baudrate Initial baudrate
 * @return true on success, false on failure
 */
bool serial_manager_init(uart_port_t uart, gpio_num_t tx_pin, gpio_num_t rx_pin, int baudrate);

/**
 * @brief Get current serial state
 * @return Current state
 */
serial_state_t serial_manager_get_state(void);

/**
 * @brief Notify web client connected
 */
void serial_manager_web_client_connected(void);

/**
 * @brief Notify web client disconnected
 */
void serial_manager_web_client_disconnected(void);

/**
 * @brief Get web client count
 * @return Number of connected web clients
 */
int serial_manager_get_web_client_count(void);

/**
 * @brief Set USB connection status
 * @param connected true if USB CDC is connected
 */
void serial_manager_set_usb_connected(bool connected);

/**
 * @brief Check if USB is connected
 * @return true if USB CDC is connected
 */
bool serial_manager_is_usb_connected(void);

/**
 * @brief Handle UART received data
 * @param data Pointer to data buffer
 * @param len Data length
 */
void serial_manager_handle_uart_data(const uint8_t *data, size_t len);

/**
 * @brief Send data to UART
 * @param data Pointer to data buffer
 * @param len Data length
 * @return true on success, false on failure
 */
bool serial_manager_send_to_uart(const uint8_t *data, size_t len);

/**
 * @brief Set baudrate
 * @param baudrate New baudrate
 * @return true on success, false on failure
 */
bool serial_manager_set_baudrate(uint32_t baudrate);

/**
 * @brief Get current baudrate
 * @param baudrate Pointer to store baudrate
 * @return true on success, false on failure
 */
bool serial_manager_get_baudrate(uint32_t *baudrate);

/**
 * @brief Get current UART port
 * @return UART port number
 */
uart_port_t serial_manager_get_uart_port(void);

/**
 * @brief Set UART configuration (data bits, parity, stop bits)
 * @param data_bits Data bits (5, 6, 7, or 8)
 * @param parity Parity (0=none, 1=odd, 2=even)
 * @param stop_bits Stop bits (1 or 2)
 * @return true on success, false on failure
 */
bool serial_manager_set_config(int data_bits, int parity, int stop_bits);

#ifdef __cplusplus
}
#endif
