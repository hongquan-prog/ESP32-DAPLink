#include "tusb_msc_storage.h"
#include <dirent.h>
#include <errno.h>
#include "esp_log.h"

static const char *TAG = "msc_disk";

static esp_err_t storage_init_spiflash(wl_handle_t *wl_handle)
{
    ESP_LOGI(TAG, "Initializing wear levelling");

    const esp_partition_t *data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, NULL);
    if (data_partition == NULL)
    {
        ESP_LOGE(TAG, "Failed to find FATFS partition. Check the partition table.");
        return ESP_ERR_NOT_FOUND;
    }

    return wl_mount(data_partition, wl_handle);
}

// mount the partition and show all the files in path
void msc_dick_mount(const char *path)
{
    static wl_handle_t wl_handle = WL_INVALID_HANDLE;
    ESP_ERROR_CHECK(storage_init_spiflash(&wl_handle));

    const tinyusb_msc_spiflash_config_t config_spi = {.wl_handle = wl_handle};
    ESP_ERROR_CHECK(tinyusb_msc_storage_init_spiflash(&config_spi));

    ESP_LOGI(TAG, "Mount storage...");

    ESP_ERROR_CHECK(tinyusb_msc_storage_mount(path));

    // List all the files in this directory
    ESP_LOGI(TAG, "\nls command output:");
    struct dirent *d;
    DIR *dh = opendir(path);
    if (!dh)
    {
        if (errno == ENOENT)
        {
            // If the directory is not found
            ESP_LOGE(TAG, "Directory doesn't exist %s", path);
        }
        else
        {
            // If the directory is not readable then throw error and exit
            ESP_LOGE(TAG, "Unable to read directory %s", path);
        }
        return;
    }
    // While the next entry is not readable we will print directory files
    while ((d = readdir(dh)) != NULL)
    {
        printf("%s\n", d->d_name);
    }
    return;
}