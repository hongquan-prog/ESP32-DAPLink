/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <driver/gpio.h>
#include "sdkconfig.h"
#include "compiler.h"

/* Debug pins - JTAG */
#define GPIO_TDI        CONFIG_DEBUG_PROBE_GPIO_TDI
#define GPIO_TDO        CONFIG_DEBUG_PROBE_GPIO_TDO
#define GPIO_TCK        CONFIG_DEBUG_PROBE_GPIO_TCK
#define GPIO_TMS        CONFIG_DEBUG_PROBE_GPIO_TMS

/* Debug pins - SWD */
#define GPIO_SWCLK      CONFIG_DEBUG_PROBE_GPIO_TCK
#define GPIO_SWDIO      CONFIG_DEBUG_PROBE_GPIO_TMS

/**
 * @brief Initialize debug GPIO pins for JTAG mode
 */
void debug_probe_init_jtag_pins(void);

/**
 * @brief Initialize debug GPIO pins for SWD mode
 */
void debug_probe_init_swd_pins(void);

/**
 * @brief Reset and cleanup debug GPIO pins
 */
void debug_probe_reset_pins(void);

/**
 * @brief Notify debug activity
 * @param active true when debug activity is happening, false when idle
 */
void debug_probe_notify_activity(bool active);

/* High-performance inline GPIO functions for debug operations */

__STATIC_FORCEINLINE void debug_probe_swdio_out_enable(void)
{
    gpio_set_direction((gpio_num_t)GPIO_SWDIO, GPIO_MODE_OUTPUT);
}

__STATIC_FORCEINLINE void debug_probe_swdio_out_disable(void)
{
    gpio_set_direction((gpio_num_t)GPIO_SWDIO, GPIO_MODE_INPUT);
}

__STATIC_FORCEINLINE int debug_probe_swdio_read(void)
{
    return gpio_get_level((gpio_num_t)GPIO_SWDIO);
}

__STATIC_FORCEINLINE void debug_probe_swclk_set(void)
{
    gpio_set_level((gpio_num_t)GPIO_SWCLK, 1);
}

__STATIC_FORCEINLINE void debug_probe_swclk_clr(void)
{
    gpio_set_level((gpio_num_t)GPIO_SWCLK, 0);
}

__STATIC_FORCEINLINE void debug_probe_swdio_set(void)
{
    gpio_set_level((gpio_num_t)GPIO_SWDIO, 1);
}

__STATIC_FORCEINLINE void debug_probe_swdio_clr(void)
{
    gpio_set_level((gpio_num_t)GPIO_SWDIO, 0);
}

__STATIC_FORCEINLINE void debug_probe_swdio_write(int val)
{
    /* val is treated as a bit value, only bit 0 is checked */
    gpio_set_level((gpio_num_t)GPIO_SWDIO, (val & 0x01) ? 1 : 0);
}

__STATIC_FORCEINLINE void debug_probe_swd_blink(int on)
{
    debug_probe_notify_activity(on);
}

__STATIC_FORCEINLINE void debug_probe_jtag_led_off(void)
{
    debug_probe_notify_activity(false);
}

__STATIC_FORCEINLINE void debug_probe_jtag_led_on(void)
{
    debug_probe_notify_activity(true);
}

__STATIC_FORCEINLINE int debug_probe_tdo_read(void)
{
    return gpio_get_level((gpio_num_t)GPIO_TDO);
}

__STATIC_FORCEINLINE void debug_probe_tdi_write(int val)
{
    /* val is treated as a bit value, only bit 0 is checked */
    gpio_set_level((gpio_num_t)GPIO_TDI, (val & 0x01) ? 1 : 0);
}

__STATIC_FORCEINLINE void debug_probe_tck_set(void)
{
    gpio_set_level((gpio_num_t)GPIO_TCK, 1);
}

__STATIC_FORCEINLINE void debug_probe_tck_clr(void)
{
    gpio_set_level((gpio_num_t)GPIO_TCK, 0);
}

__STATIC_FORCEINLINE void debug_probe_write_tmstck(uint8_t tms_tdi_mask)
{
    gpio_set_level((gpio_num_t)GPIO_TMS, (tms_tdi_mask >> 2) & 1);
    gpio_set_level((gpio_num_t)GPIO_TDI, (tms_tdi_mask >> 1) & 1);
}

/* Basic GPIO mode functions for debug pins */
__STATIC_FORCEINLINE void debug_probe_mode_input_enable(int gpio_num)
{
    gpio_set_direction((gpio_num_t)gpio_num, GPIO_MODE_INPUT);
}

__STATIC_FORCEINLINE void debug_probe_mode_out_enable(int gpio_num)
{
    gpio_set_direction((gpio_num_t)gpio_num, GPIO_MODE_OUTPUT);
}

__STATIC_FORCEINLINE void debug_probe_mode_in_out_enable(int gpio_num)
{
    gpio_set_direction((gpio_num_t)gpio_num, GPIO_MODE_INPUT_OUTPUT);
}

__STATIC_FORCEINLINE void debug_probe_set(int gpio_num)
{
    gpio_set_level((gpio_num_t)gpio_num, 1);
}

__STATIC_FORCEINLINE void debug_probe_clear(int gpio_num)
{
    gpio_set_level((gpio_num_t)gpio_num, 0);
}