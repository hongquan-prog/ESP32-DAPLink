/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   add license declaration
 * 2026-3-17     refactor     Add USB connection state callback
 */
#pragma once

#include "tinyusb.h"
#include "tusb_cdc_acm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Send data from UART to USB CDC host
 * @param context USB CDC context pointer (cast to int for CDC port)
 * @param data Pointer to data buffer
 * @param size Size of data to send
 */
void usb_cdc_send_to_host(void *context, uint8_t *data, size_t size);

/**
 * @brief Handle USB CDC receive event and forward to UART
 * @param itf CDC interface number
 * @param event CDC ACM event structure
 */
void usb_cdc_send_to_uart(int itf, cdcacm_event_t *event);

/**
 * @brief Handle USB CDC line coding change event
 * @param itf CDC interface number
 * @param event CDC ACM event structure containing new line coding
 */
void usb_cdc_set_line_codinig(int itf, cdcacm_event_t *event);

/**
 * @brief Handle USB CDC line state change event (DTR/RTS)
 * @param itf CDC interface number
 * @param event CDC ACM event structure
 */
void usb_cdc_line_state_changed(int itf, cdcacm_event_t *event);

#ifdef __cplusplus
}
#endif