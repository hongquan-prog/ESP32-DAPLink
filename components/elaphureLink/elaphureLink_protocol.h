/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   Initial version
 * 2026-3-17     refactor     Code style compliance fixes
 * 2026-3-17     refactor     Remove upper layer dependency, output via parameters
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

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
 * @brief Check if buffer is elaphureLink handshake request
 *
 * @param buffer Packet buffer
 * @param len Packet length
 * @return true if it's a valid elaphureLink handshake request
 */
bool el_is_handshake_request(const void *buffer, size_t len);

/**
 * @brief Create handshake response
 *
 * @param response Output buffer for response
 * @return Response size
 */
size_t el_create_handshake_response(el_response_handshake_t *response);

/**
 * @brief Process DAP command and get response
 *
 * @param buffer DAP command buffer
 * @param len DAP command length (unused, DAP commands are fixed size)
 * @param response Output buffer for response (must be at least 1500 bytes)
 * @param response_size Output: response size
 */
void el_process_dap_command(const void *buffer, size_t len, uint8_t *response, size_t *response_size);

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