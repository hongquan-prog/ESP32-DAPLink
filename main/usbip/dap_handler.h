/**
 * @file dap_handler.h
 * @brief DAP endpoint handler interface
 *
 * Handles DAP data processing for endpoint 1 (EP1).
 * Provides DAP response buffer management.
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize DAP endpoint handler
 *
 * Registers the DAP handler for endpoint 1 (EP1).
 */
void dap_handler_init(void);

/* ========== DAP Response Buffer API ========== */

/**
 * @brief Check if DAP response is ready
 *
 * @return true if response is available, false otherwise
 */
bool dap_is_response_ready(void);

/**
 * @brief Get DAP response data
 *
 * @param data Output: pointer to response data
 * @param length Output: data length
 * @return true if response was retrieved, false if no response available
 */
bool dap_get_response(uint8_t **data, uint32_t *length);

/**
 * @brief Clear DAP response (mark as consumed)
 */
void dap_clear_response(void);

#ifdef __cplusplus
}
#endif