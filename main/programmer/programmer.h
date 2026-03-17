/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   Initial version
 * 2026-3-16     Refactor     Improved documentation
 */
#pragma once

#include "programmer/prog_data.h"

/**
 * @file programmer.h
 * @brief Programmer module public interface
 * 
 * This file defines the public API for the programmer module.
 */

/**
 * @brief Initialize programmer module
 * 
 * Creates the programmer task and initializes directories.
 */
void programmer_init(void);

/**
 * @brief Handle programming request
 * @param buf JSON request buffer
 * @param len Buffer length
 * @return Error code
 */
prog_err_def programmer_request_handle(char *buf, int len);

/**
 * @brief Get programmer status
 * @param buf Output buffer
 * @param size Buffer size
 * @param encode_len Output: encoded length
 */
void programmer_get_status(char *buf, int size, int &encode_len);

/**
 * @brief Write programming data
 * @param data Data buffer
 * @param len Data length
 * @return Error code
 */
prog_err_def programmer_write_data(uint8_t *data, int len);
