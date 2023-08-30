#pragma once

#include "tinyusb.h"
#include "tusb_cdc_acm.h"

#ifdef __cplusplus
extern "C" {
#endif

void cdc_rx_callback(int itf, cdcacm_event_t *event);
void cdc_line_codinig_changed_callback(int itf, cdcacm_event_t *event);
void cdc_uart_rx_callback(int uart, void *usr_data, uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif