/**
 * @file swo_handler.h
 * @brief SWO Trace endpoint handler interface
 *
 * Handles SWO trace data for endpoint 2 (EP2).
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize SWO endpoint handler
 *
 * Registers the SWO handler for endpoint 2 (EP2).
 */
void swo_handler_init(void);

/**
 * @brief SWO Data Queue Transfer
 *
 * @param buf Pointer to buffer with data
 * @param num Number of bytes to transfer
 */
void SWO_QueueTransfer(uint8_t *buf, uint32_t num);

#ifdef __cplusplus
}
#endif