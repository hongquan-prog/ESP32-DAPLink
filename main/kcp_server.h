/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-10-08    windows       Initial version
 * 2026-03-17    lihongquan    Refactored according to code style guide
 */

#pragma once

/**
 * @file kcp_server.h
 * @brief KCP server task and network send interface
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief KCP server main task
 *
 * This task handles KCP protocol communication over UDP.
 * It manages socket creation, KCP initialization, and data processing.
 */
void kcp_server_task(void);

/**
 * @brief Send data through KCP connection
 *
 * @param buffer Pointer to data buffer
 * @param len Length of data to send
 * @return 0 on success
 */
int kcp_network_send(const char *buffer, int len);

#ifdef __cplusplus
}
#endif