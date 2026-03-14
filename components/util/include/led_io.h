/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* LEDs */
#define LED_TX          CONFIG_BRIDGE_GPIO_LED1
#define LED_RX          CONFIG_BRIDGE_GPIO_LED2

#define LED_TX_ON       CONFIG_BRIDGE_GPIO_LED1_ACTIVE
#define LED_TX_OFF      (!CONFIG_BRIDGE_GPIO_LED1_ACTIVE)

#define LED_RX_ON       CONFIG_BRIDGE_GPIO_LED2_ACTIVE
#define LED_RX_OFF      (!CONFIG_BRIDGE_GPIO_LED2_ACTIVE)

#define LED_JTAG_ON     
#define LED_JTAG_OFF    
