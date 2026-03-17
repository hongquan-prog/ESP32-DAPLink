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
 * @file tcp_server.h
 * @brief TCP server task interface
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief TCP server main task
 *
 * @param pvParameters Task parameters (unused)
 */
void tcp_server_task(void *pvParameters);

#ifdef __cplusplus
}
#endif
