/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-4-20     lihongquan   WiFi configuration via web page
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check if WiFi SSID exists in NVS
 * @return true if SSID exists in NVS, false otherwise
 */
bool wifi_config_exists(void);

/**
 * @brief Start AP mode for configuration
 */
void wifi_init_softap(void);

/**
 * @brief Connect to WiFi using credentials from NVS
 */
void wifi_connect_sta(void);

#ifdef __cplusplus
}
#endif
