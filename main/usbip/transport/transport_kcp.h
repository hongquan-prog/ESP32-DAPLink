/**
 * @file transport_kcp.h
 * @brief KCP (reliable UDP) transport interface
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
 * @brief Get KCP transport operations
 *
 * @return Pointer to KCP transport operations table
 */
const transport_ops_t *transport_kcp_get_ops(void);

#ifdef __cplusplus
}
#endif