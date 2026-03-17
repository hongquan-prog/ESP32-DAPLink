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

#include <stdint.h>
#include <stddef.h>

/**
 * @file tcp_netconn.h
 * @brief TCP netconn server interface
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Send data through TCP netconn connection
 *
 * @param buffer Pointer to data buffer
 * @param len Length of data to send
 * @return 0 on success, negative value on error
 */
int tcp_netconn_send(const void *buffer, size_t len);

/**
 * @brief TCP netconn server main task
 *
 * This task handles TCP communication using LwIP netconn API.
 */
void tcp_netconn_task(void);

#ifdef __cplusplus
}
#endif