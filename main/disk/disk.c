/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   add license declaration
 */

#include "disk.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "wear_levelling.h"
#include "esp_vfs_fat.h"
#include "driver/gpio.h"

#if defined(CONFIG_SOC_SDMMC_HOST_SUPPORTED)
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#endif

#include "esp_check.h"

static const char *TAG = "disk";

esp_err_t storage_init_spiflash(wl_handle_t *wl_handle)
{
    ESP_LOGI(TAG, "Initializing wear levelling");

    const esp_partition_t *data_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, NULL);
    if (data_partition == NULL)
    {
        ESP_LOGE(TAG, "Failed to find FATFS partition. Check the partition table.");
        return ESP_ERR_NOT_FOUND;
    }

    return wl_mount(data_partition, wl_handle);
}

#if defined(CONFIG_SOC_SDMMC_HOST_SUPPORTED)
esp_err_t storage_init_sdmmc(sdmmc_card_t **card)
{
    esp_err_t ret = ESP_FAIL;
    bool host_init = false;
    sdmmc_card_t *sd_card;

    ESP_LOGI(TAG, "Initializing SDCard");

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 40MHz for SDMMC)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // For SD Card, set bus width to use
    slot_config.width = 4;

#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32P4)
    slot_config.clk = GPIO_NUM_36;
    slot_config.cmd = GPIO_NUM_35;
    slot_config.d0 = GPIO_NUM_37;
    slot_config.d1 = GPIO_NUM_38;
    slot_config.d2 = GPIO_NUM_33;
    slot_config.d3 = GPIO_NUM_34;
#endif

    // Enable internal pullups on enabled pins. The internal pullups
    // are insufficient however, please make sure 10k external pullups are
    // connected on the bus. This is for debug / example purpose only.
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    // not using ff_memalloc here, as allocation in internal RAM is preferred
    sd_card = (sdmmc_card_t *)malloc(sizeof(sdmmc_card_t));
    ESP_GOTO_ON_FALSE(sd_card, ESP_ERR_NO_MEM, clean, TAG, "could not allocate new sdmmc_card_t");

    ESP_GOTO_ON_ERROR((*host.init)(), clean, TAG, "Host Config Init fail");
    host_init = true;

    ESP_GOTO_ON_ERROR(sdmmc_host_init_slot(host.slot, (const sdmmc_slot_config_t *)&slot_config),
                      clean, TAG, "Host init slot fail");

    if (sdmmc_card_init(&host, sd_card) != ESP_OK)
    {
        ESP_LOGE(TAG, "The detection pin of the slot is disconnected(Insert uSD card)");
        goto clean;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, sd_card);
    *card = sd_card;

    return ESP_OK;

clean:
    if (host_init)
    {
        if (host.flags & SDMMC_HOST_FLAG_DEINIT_ARG)
        {
            host.deinit_p(host.slot);
        }
        else
        {
            (*host.deinit)();
        }
    }

    if (sd_card)
    {
        free(sd_card);
        sd_card = NULL;
    }

    return ret;
}
#endif

esp_err_t mount_spiflash_fs(void)
{
    ESP_LOGI(TAG, "Mounting FAT filesystem");

    const esp_partition_t *data_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, NULL);
    if (data_partition == NULL)
    {
        ESP_LOGE(TAG, "Failed to find FATFS partition");
        return ESP_ERR_NOT_FOUND;
    }

    wl_handle_t wl_handle = WL_INVALID_HANDLE;
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
        .disk_status_check_enable = false,
    };

    esp_err_t ret = esp_vfs_fat_spiflash_mount_rw_wl("/data", data_partition->label, &mount_config, &wl_handle);
    if (ret == ESP_ERR_INVALID_STATE)
    {
        ESP_LOGD(TAG, "FATFS already registered");
        ret = ESP_OK;
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_vfs_fat_spiflash_mount_rw_wl failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "FAT filesystem mounted at /data");
    return ESP_OK;
}