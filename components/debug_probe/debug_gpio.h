/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"

#if CONFIG_DEBUG_PROBE_USE_DEDICATED_GPIO
#include "debug_gpio_dedic.h"
#else
#include "debug_gpio_reg.h"
#endif