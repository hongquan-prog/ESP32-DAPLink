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

#include "tinyusb.h"
#include "tusb_cdc_acm.h"

#ifdef __cplusplus
extern "C" {
#endif

void usb_cdc_send_to_host(void *context, uint8_t *data, size_t size);
void usb_cdc_send_to_uart(int itf, cdcacm_event_t *event);
void usb_cdc_set_line_codinig(int itf, cdcacm_event_t *event);

#ifdef __cplusplus
}
#endif