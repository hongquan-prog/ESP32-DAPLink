/**
 * @file usbip_config.h
 * @brief USBIP server configuration settings
 *
 * @copyright Copyright (c) 2020
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/*
 * Transport Configuration
 */
#define USBIP_USE_TCP_NETCONN 0
#define USBIP_PORT            3240
#define USBIP_USE_IPV4        1
#define USBIP_USE_KCP         0
#define USBIP_MTU_SIZE        1500

#if (USBIP_USE_TCP_NETCONN == 1 && USBIP_USE_KCP == 1)
#error "Cannot use KCP and TCP netconn at the same time!"
#endif

#if (USBIP_USE_KCP == 1)
#warning "KCP is an experimental feature. Related usbip version: https://github.com/windowsair/usbip-win"
#endif