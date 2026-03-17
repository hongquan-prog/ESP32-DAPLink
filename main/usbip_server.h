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

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    USBIP_STATE_ACCEPTING,
    USBIP_STATE_ATTACHING,
    USBIP_STATE_EMULATING,
    USBIP_STATE_EL_DATA_PHASE
} usbip_state_t;

extern int kSock;

/**
 * @brief Get current USBIP state
 *
 * @return Current state value
 */
usbip_state_t usbip_get_state(void);

/**
 * @brief Set USBIP state
 *
 * @param state State value to set
 */
void usbip_set_state(usbip_state_t state);

/**
 * @brief Handle USBIP attach stage
 *
 * @param buffer Data buffer
 * @param length Data length
 * @return 0 on success, negative on error
 */
int usbip_attach(uint8_t *buffer, uint32_t length);

/**
 * @brief Handle USBIP emulate stage
 *
 * @param buffer Data buffer
 * @param length Data length
 * @return 0 on success, negative on error
 */
int usbip_emulate(uint8_t *buffer, uint32_t length);

/**
 * @brief Send stage2 submit response with data
 *
 * @param req_header Request header
 * @param status Status code
 * @param data Data to send
 * @param data_length Data length
 */
void usbip_send_stage2_submit_data(usbip_stage2_header *req_header, int32_t status, const void *const data, int32_t data_length);

/**
 * @brief Send stage2 submit response
 *
 * @param req_header Request header
 * @param status Status code
 * @param data_length Data length
 */
void usbip_send_stage2_submit(usbip_stage2_header *req_header, int32_t status, int32_t data_length);

/**
 * @brief Send stage2 submit response with data (fast path)
 *
 * @param req_header Request header
 * @param data Data to send
 * @param data_length Data length
 */
void usbip_send_stage2_submit_data_fast(usbip_stage2_header *req_header, const void *const data, int32_t data_length);

/**
 * @brief Send data through USBIP network
 *
 * @param s Socket descriptor
 * @param dataptr Data pointer
 * @param size Data size
 * @param flags Send flags
 * @return Bytes sent or negative on error
 */
int usbip_network_send(int s, const void *dataptr, size_t size, int flags);

#ifdef __cplusplus
}
#endif