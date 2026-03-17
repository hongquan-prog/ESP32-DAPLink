/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-10-08    windows       Initial version
 * 2026-03-17    lihongquan    Refactored according to code style guide
 */

#pragma once

#include "components/USBIP/usbip_defs.h"
#include "FreeRTOS.h"
#include "freertos/task.h"

typedef enum
{
    DAP_SIGNAL_NONE = 0,
    DAP_SIGNAL_RESET = 1,
    DAP_SIGNAL_DELETE = 2,
} dap_signal_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set DAP restart signal
 *
 * @param signal Signal type (DAP_SIGNAL_NONE, DAP_SIGNAL_RESET, DAP_SIGNAL_DELETE)
 */
void dap_set_restart_signal(dap_signal_t signal);

/**
 * @brief Get current DAP restart signal
 *
 * @return Current signal value
 */
dap_signal_t dap_get_restart_signal(void);

/**
 * @brief Clear DAP restart signal to none
 */
void dap_clear_restart_signal(void);

/**
 * @brief Initialize DAP task
 */
void dap_task_init(void);

/**
 * @brief Get DAP task handle
 *
 * @return Current DAP task handle
 */
TaskHandle_t dap_get_task_handle(void);

/**
 * @brief Notify DAP task
 *
 * This function sends a notification to the DAP task.
 */
void dap_notify_task(void);

/**
 * @brief Handle DAP data request from USBIP
 * @param header USBIP stage2 header
 * @param length Total length of the request
 */
void handle_dap_data_request(usbip_stage2_header *header, uint32_t length);

/**
 * @brief Handle DAP data response to USBIP
 * @param header USBIP stage2 header
 */
void handle_dap_data_response(usbip_stage2_header *header);

/**
 * @brief Handle SWO trace response
 * @param header USBIP stage2 header
 */
void handle_swo_trace_response(usbip_stage2_header *header);

/**
 * @brief Handle DAP unlink request
 */
void handle_dap_unlink(void);

/**
 * @brief DAP task main function
 * @param argument Task argument (unused)
 */
void dap_task(void *argument);

/**
 * @brief Generate fast reply for DAP commands
 * @param buf Output buffer for reply
 * @param length Buffer length
 * @return Number of bytes written, 0 if no fast reply available
 */
int fast_reply(uint8_t *buf, uint32_t length);

#ifdef __cplusplus
}
#endif