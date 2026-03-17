/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   add license declaration
 */
#pragma once 

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mount the MSC disk at the specified path
 * @param path Mount path for the MSC disk
 * @return true on success, false on failure
 */
bool msc_dick_mount(const char *path);

#ifdef __cplusplus
}
#endif