/**
 * @file usbip_server.h
 * @brief USBIP server legacy compatibility layer
 *
 * This module provides legacy API for backward compatibility.
 * New code should use usbip_protocol.h instead.
 *
 * @copyright Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#include "components/USBIP/usbip_defs.h"
#include "usbip/usbip_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Legacy state API ========== */

/**
 * @brief Get current USBIP state (legacy, uses global)
 */
usbip_state_t usbip_get_state(void);

/**
 * @brief Set USBIP state (legacy, uses global)
 */
void usbip_set_state(usbip_state_t state);

/* ========== Legacy context API ========== */

/**
 * @brief Set global context for legacy API compatibility
 */
void usbip_set_global_context(usbip_context_t *ctx);

/**
 * @brief Get global context for legacy API compatibility
 */
usbip_context_t *usbip_get_global_context(void);

/* ========== Legacy network API ========== */

/**
 * @brief Send data through USBIP network (legacy, uses global context)
 *
 * @param s Socket descriptor (ignored, uses global)
 * @param dataptr Data pointer
 * @param size Data size
 * @param flags Send flags
 * @return Bytes sent or negative on error
 */
int usbip_network_send(int s, const void *dataptr, size_t size, int flags);

#ifdef __cplusplus
}
#endif