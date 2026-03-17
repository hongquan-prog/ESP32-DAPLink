/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-3-17      refactor     Convert to Kconfig-based configuration
 */

#pragma once

#include "sdkconfig.h"

/**
 * @brief USBIP Configuration
 *
 * These options are configured via menuconfig:
 * - CONFIG_USBIP_ENABLE_WINUSB: Enable WinUSB mode
 * - CONFIG_USBIP_ENABLE_USB_3_0: Enable USB 3.0 speed
 * - CONFIG_USBIP_ENDPOINT_SIZE: Endpoint size
 */

#ifdef CONFIG_USBIP_ENABLE_WINUSB
#define USBIP_ENABLE_WINUSB 1
#else
#define USBIP_ENABLE_WINUSB 0
#endif

#ifdef CONFIG_USBIP_ENABLE_USB_3_0
#define USBIP_ENABLE_USB_3_0 1
#else
#define USBIP_ENABLE_USB_3_0 0
#endif

/**
 * @brief USB endpoint size
 *
 * Automatically set based on USB speed configuration:
 * - USB 3.0: 1024 bytes
 * - USB 2.0: 512 bytes
 */
#define USBIP_ENDPOINT_SIZE CONFIG_USBIP_ENDPOINT_SIZE