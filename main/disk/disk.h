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

#include <stdint.h>
#include "esp_err.h"
#include "driver/sdmmc_types.h"
#include "wear_levelling.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize SPI flash storage with wear levelling
 * @param wl_handle Output handle for the wear levelling instance
 * @return ESP_OK on success, error code on failure
 */
esp_err_t storage_init_spiflash(wl_handle_t *wl_handle);

/**
 * @brief Initialize SD card storage
 * @param card Output pointer to the SD card structure
 * @return ESP_OK on success, error code on failure
 */
#if defined(CONFIG_SOC_SDMMC_HOST_SUPPORTED)
esp_err_t storage_init_sdmmc(sdmmc_card_t **card);
#endif

/**
 * @brief Mount FAT filesystem on SPI flash
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mount_spiflash_fs(void);

#ifdef __cplusplus
}
#endif