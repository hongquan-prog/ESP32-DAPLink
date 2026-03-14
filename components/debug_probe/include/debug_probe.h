/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <freertos/FreeRTOS.h>

/* Be careful if you want to change the index. It must be bigger then the size of string_desc_arr
   For now 7 would be also ok. But lets reserve some fields for the future additions
   Also it must be match with the value in the openocd/esp_usb_bridge.cfg file
   Currently it is defined as < esp_usb_jtag_caps_descriptor 0x030A >
*/
#define DEBUG_PROBE_STR_DESC_INX    0x0A

// Maximum packet size for the debug probe command and response data.
// Typical vales are 64 for Full-speed USB HID or WinUSB,
// 1024 for High-speed USB HID and 512 for High-speed USB WinUSB.
// In case of SWD, the value needs to be the same as DAP_PACKET_SIZE in DAP_config.h
#define DEBUG_PROBE_PACKET_SIZE 64

#define DEBUG_PROBE_TASK_PRI        5

typedef struct {
    bool success;
    uint8_t *data;
    size_t data_len;
} debug_probe_cmd_response_t;

/**
 * @brief Initialize the debug probe subsystem
 *
 * This function initializes the debug probe functionality, including SWD/JTAG
 * interfaces, task creation, and internal data structures. Must be called
 * before any other debug probe operations.
 *
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t debug_probe_init(void);

/**
 * @brief Get debug probe protocol capabilities
 *
 * Retrieves the protocol capabilities of the debug probe, which describes
 * supported features and protocols (e.g., SWD, JTAG versions, speeds).
 * This information is typically used by debugger software like OpenOCD.
 *
 * @param dest Destination buffer to store capabilities data
 * @return int Number of bytes written to destination buffer, or negative on error
 */
int debug_probe_get_proto_caps(void *dest);

/**
 * @brief Handle ESP32 TDI bootstrapping during reboot sequences
 *
 * This function manages the ESP32-specific TDI pin control during reboot sequences
 * to prevent flash voltage issues. On ESP32, TDI (GPIO12) is also a strapping pin
 * that determines flash voltage. If TDI is high when ESP32 is released from reset,
 * the flash voltage is set to 1.8V and the chip will fail to boot.
 *
 * @param rebooting true when entering reboot sequence, false when reboot sequence is complete
 */
void debug_probe_handle_esp32_tdi_bootstrapping(bool rebooting);

/**
 * @brief Handle debug probe command
 *
 * Processes a debug probe command with the specified command code and value.
 * This is typically called in response to USB control transfers or similar
 * command protocols from debugging software.
 *
 * @param cmd Command code to execute
 * @param wValue Command-specific value parameter
 * @return debug_probe_cmd_response_t Response structure containing success status and optional data
 */
debug_probe_cmd_response_t debug_probe_handle_command(uint8_t cmd, uint16_t wValue);

/**
 * @brief Process incoming debug probe data
 *
 * Processes raw data received from the host debugger software. This function
 * parses and handles debug commands, SWD/JTAG transactions, and other
 * debug-related communications.
 *
 * @param data Received data buffer
 * @param len Length of received data
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t debug_probe_process_data(const uint8_t *data, size_t len);

/**
 * @brief Get data ready to be sent to host
 *
 * Retrieves debug probe response data that is ready to be transmitted
 * back to the host debugger. This function may block waiting for data
 * to become available.
 *
 * @param len Pointer to store the length of returned data
 * @param timeout Maximum time to wait for data (FreeRTOS ticks)
 * @return uint8_t* Pointer to data buffer, or NULL if no data available within timeout
 */
uint8_t *debug_probe_get_data_to_send(size_t *len, TickType_t timeout);

/**
 * @brief Free previously sent data buffer
 *
 * Releases a data buffer that was previously obtained from
 * debug_probe_get_data_to_send() and has been successfully transmitted.
 * This allows the debug probe to reuse or deallocate the buffer.
 *
 * @param data Pointer to data buffer to free
 */
void debug_probe_free_sent_data(uint8_t *data);

/**
 * @brief Debug activity notification callback
 * @param active true when debug activity is happening, false when idle
 */
typedef void (*debug_activity_notify_cb_t)(bool active);

/**
 * @brief Register callback for debug activity notifications
 *
 * This callback will be called to indicate debug probe activity (JTAG/SWD operations).
 * Useful for driving activity LEDs or other status indicators.
 *
 * @param callback Callback function, can be NULL to disable notifications
 */
void debug_probe_register_activity_callback(debug_activity_notify_cb_t callback);
