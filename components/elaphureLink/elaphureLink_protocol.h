/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   Initial version
 * 2026-3-17     refactor     Code style compliance fixes
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EL_LINK_IDENTIFIER 0x8a656c70
#define EL_DAP_VERSION 0x00000001
#define EL_COMMAND_HANDSHAKE 0x00000000

/**
 * @brief elaphureLink handshake request structure
 */
typedef struct
{
    uint32_t el_link_identifier;    ///< Link identifier
    uint32_t command;               ///< Command type
    uint32_t el_proxy_version;      ///< Proxy version
} __attribute__((packed)) el_request_handshake_t;

/**
 * @brief elaphureLink handshake response structure
 */
typedef struct
{
    uint32_t el_link_identifier;    ///< Link identifier
    uint32_t command;               ///< Command type
    uint32_t el_dap_version;        ///< DAP version
} __attribute__((packed)) el_response_handshake_t;

/**
 * @brief elaphureLink Proxy handshake phase process
 * @param fd socket fd
 * @param buffer packet buffer
 * @param len packet length
 * @return 0 on Success, other on failed
 */
int el_handshake_process(int fd, void *buffer, size_t len);

/**
 * @brief Process dap data and send to socket
 * @param buffer dap data buffer
 * @param len dap data length
 */
void el_dap_data_process(void *buffer, size_t len);

/**
 * @brief Allocate process buffer for elaphureLink
 */
void el_process_buffer_malloc(void);

/**
 * @brief Free process buffer for elaphureLink
 */
void el_process_buffer_free(void);

#ifdef __cplusplus
}
#endif