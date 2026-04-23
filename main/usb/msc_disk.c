/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   add license declaration
 */

#include "tinyusb.h"
#include "tusb_msc_storage.h"
#include <dirent.h>
#include <errno.h>
#include "esp_log.h"
#include "disk.h"
#include "esp_check.h"

static const char *TAG = "msc_disk";

// mount the partition and show all the files in path
bool msc_dick_mount(const char *path)
{
#ifdef CONFIG_MSC_STORAGE_MEDIA_SPIFLASH
    static wl_handle_t wl_handle = WL_INVALID_HANDLE;
    ESP_ERROR_CHECK(storage_init_spiflash(&wl_handle));

    const tinyusb_msc_spiflash_config_t config_spi = {.wl_handle = wl_handle};
    ESP_ERROR_CHECK(tinyusb_msc_storage_init_spiflash(&config_spi));
#else
    static sdmmc_card_t *card = NULL;

    if (storage_init_sdmmc(&card) != ESP_OK)
        return false;

    const tinyusb_msc_sdmmc_config_t config_sdmmc = {
        .card = card};

    ESP_ERROR_CHECK(tinyusb_msc_storage_init_sdmmc(&config_sdmmc));
#endif
    ESP_LOGI(TAG, "Mount storage...");

    ESP_ERROR_CHECK(tinyusb_msc_storage_mount(path));
    return true;
}