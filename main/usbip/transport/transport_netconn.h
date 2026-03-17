/**
 * @file transport_netconn.h
 * @brief LwIP Netconn transport interface
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
 * @brief Get Netconn transport operations
 *
 * @return Pointer to Netconn transport operations table
 */
const transport_ops_t *transport_netconn_get_ops(void);

#ifdef __cplusplus
}
#endif