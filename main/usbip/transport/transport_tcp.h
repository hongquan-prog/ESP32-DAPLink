/**
 * @file transport_tcp.h
 * @brief BSD Socket TCP transport interface
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "usbip/transport/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get TCP transport operations
 *
 * @return Pointer to TCP transport operations table
 */
const transport_ops_t *transport_tcp_get_ops(void);

#ifdef __cplusplus
}
#endif