/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debug_gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "debug_probe.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))
#endif

static int s_debug_gpio_init = 0;

static const char *TAG = "debug_gpio_reg";

static debug_activity_notify_cb_t s_activity_callback = NULL;

void debug_probe_register_activity_callback(debug_activity_notify_cb_t callback)
{
    s_activity_callback = callback;
}

void debug_probe_notify_activity(bool active)
{
    if (s_activity_callback) {
        s_activity_callback(active);
    }
}

#if CONFIG_DEBUG_PROBE_IFACE_JTAG

void debug_probe_init_jtag_pins(void)
{
    if (!s_debug_gpio_init) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = BIT64(GPIO_TDI) | BIT64(GPIO_TCK) | BIT64(GPIO_TMS),
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&io_conf));

        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = BIT64(GPIO_TDO);
        ESP_ERROR_CHECK(gpio_config(&io_conf));

        gpio_set_level((gpio_num_t)GPIO_TMS, 1);
        gpio_set_level((gpio_num_t)GPIO_TCK, 0);

        s_debug_gpio_init = 1;

        ESP_LOGI(TAG, "JTAG GPIO (regular) init done");
    }
}

#else

void debug_probe_init_swd_pins(void)
{
    if (!s_debug_gpio_init) {
        gpio_reset_pin(GPIO_SWDIO);
        gpio_reset_pin(GPIO_SWCLK);
        debug_probe_mode_in_out_enable(GPIO_SWDIO);
        gpio_set_pull_mode(GPIO_SWDIO, GPIO_PULLUP_ONLY);
        debug_probe_mode_out_enable(GPIO_SWCLK);

        gpio_set_level((gpio_num_t)GPIO_SWCLK, 1);
        gpio_set_level((gpio_num_t)GPIO_SWDIO, 1);

        s_debug_gpio_init = 1;

        ESP_LOGI(TAG, "SWD GPIO (regular) init done");
    }
}

void debug_probe_reset_pins(void)
{
    if (!s_debug_gpio_init) {
        return;
    }

    gpio_reset_pin(GPIO_SWDIO); // GPIO_TMS
    gpio_reset_pin(GPIO_SWCLK); // GPIO_TCK
    gpio_reset_pin(GPIO_TDI);
    gpio_reset_pin(GPIO_TDO);

    s_debug_gpio_init = 0;
}

#endif