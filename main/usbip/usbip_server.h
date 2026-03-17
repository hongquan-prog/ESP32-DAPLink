/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   Initial version
 * 2026-3-17     lihongquan   Refactored according to CODE_STYLE.md
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#include "components/USBIP/usbip_defs.h"
#include "usbip/usbip_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get current USBIP state (legacy, uses global)
 *
 * @return Current state value
 */
usbip_state_t usbip_get_state(void);

/**
 * @brief Set USBIP state (legacy, uses global)
 *
 * @param state State value to set
 */
void usbip_set_state(usbip_state_t state);

/**
 * @brief Handle USBIP attach stage (legacy, uses global socket)
 *
 * @param buffer Data buffer
 * @param length Data length
 * @return 0 on success, negative on error
 */
int usbip_attach(uint8_t *buffer, uint32_t length);

/**
 * @brief Handle USBIP emulate stage (legacy, uses global socket)
 *
 * @param buffer Data buffer
 * @param length Data length
 * @return 0 on success, negative on error
 */
int usbip_emulate(uint8_t *buffer, uint32_t length);

/**
 * @brief Send stage2 submit response with data (legacy)
 *
 * @param req_header Request header
 * @param status Status code
 * @param data Data to send
 * @param data_length Data length
 */
void usbip_send_stage2_submit_data(usbip_stage2_header *req_header, int32_t status, const void *const data, int32_t data_length);

/**
 * @brief Send stage2 submit response (legacy)
 *
 * @param req_header Request header
 * @param status Status code
 * @param data_length Data length
 */
void usbip_send_stage2_submit(usbip_stage2_header *req_header, int32_t status, int32_t data_length);

/**
 * @brief Send stage2 submit response with data - fast path (legacy)
 *
 * @param req_header Request header
 * @param data Data to send
 * @param data_length Data length
 */
void usbip_send_stage2_submit_data_fast(usbip_stage2_header *req_header, const void *const data, int32_t data_length);

/**
 * @brief Send data through USBIP network (legacy)
 *
 * @param s Socket descriptor (ignored, uses global)
 * @param dataptr Data pointer
 * @param size Data size
 * @param flags Send flags
 * @return Bytes sent or negative on error
 */
int usbip_network_send(int s, const void *dataptr, size_t size, int flags);

/* ========== Context-aware API (new) ========== */

/**
 * @brief Set global context for legacy API compatibility
 *
 * @param ctx USBIP context
 */
void usbip_set_global_context(usbip_context_t *ctx);

/**
 * @brief Get global context for legacy API compatibility
 *
 * @return USBIP context or NULL
 */
usbip_context_t *usbip_get_global_context(void);

/**
 * @brief Send stage2 submit response with data (context-aware)
 *
 * @param ctx USBIP context
 * @param req_header Request header
 * @param status Status code
 * @param data Data to send
 * @param data_length Data length
 */
void usbip_send_stage2_submit_data_ctx(usbip_context_t *ctx, usbip_stage2_header *req_header, int32_t status, const void *const data, int32_t data_length);

/**
 * @brief Send stage2 submit response (context-aware)
 *
 * @param ctx USBIP context
 * @param req_header Request header
 * @param status Status code
 * @param data_length Data length
 */
void usbip_send_stage2_submit_ctx(usbip_context_t *ctx, usbip_stage2_header *req_header, int32_t status, int32_t data_length);

/**
 * @brief Send stage2 submit response with data - fast path (context-aware)
 *
 * @param ctx USBIP context
 * @param req_header Request header
 * @param data Data to send
 * @param data_length Data length
 */
void usbip_send_stage2_submit_data_fast_ctx(usbip_context_t *ctx, usbip_stage2_header *req_header, const void *const data, int32_t data_length);

/**
 * @brief Fast reply for DAP data (context-aware)
 *
 * @param ctx USBIP context
 * @param buf Data buffer
 * @param length Data length
 * @return 1 if handled, 0 if not a fast reply
 */
int fast_reply_ctx(usbip_context_t *ctx, uint8_t *buf, uint32_t length);

#ifdef __cplusplus
}
#endif
