#include "esp_log.h"
#include "bin_prog.h"

#define TAG "bin_prog"

BinProg::BinProg()
    : _file_path(),
      _flash_manager(FlashManager::get_instance())
{
}

BinProg::BinProg(const std::string &file)
    : _file_path(file), _flash_manager(FlashManager::get_instance())
{
}

bool BinProg::programming_bin(const target_cfg_t &cfg, uint32_t addr, const std::string &file)
{
    size_t rd_size = 0;
    dap_err_t err = ERROR_SUCCESS;
    FRESULT ret = FR_OK;
    uint32_t wr_addr = addr;
    uint32_t total_size = 0;
    FILE *fp = nullptr;

    err = _flash_manager.init(&cfg);
    if (err != ERROR_SUCCESS)
    {
        return false;
    }

    ESP_LOGI(TAG, "Starting to program bin at 0x%lx", addr);

    if (file.empty())
    {
        ESP_LOGE(TAG, "No file specified");
        return false;
    }

    _file_path = file;
    fp = fopen(file.c_str(), "r");
    if (ret != FR_OK)
    {
        ESP_LOGE(TAG, "Failed to open %s", file.c_str());
        return false;
    }

    while (feof(fp) == 0)
    {
        rd_size = fread(_bin_buffer, 1, sizeof(_bin_buffer), fp);

        if (rd_size > 0)
        {
            total_size += rd_size;
            if (ERROR_SUCCESS != _flash_manager.write(wr_addr, _bin_buffer, rd_size))
            {
                ESP_LOGE(TAG, "Failed to write bin at:%lx", wr_addr);
                _flash_manager.uninit();
                return false;
            }

            wr_addr += rd_size;
        }
    }

    fclose(fp);
    _flash_manager.uninit();
    ESP_LOGI(TAG, "DAPLink write %ld bytes to %s at 0x%08lx successfully", total_size, cfg.target_part_number, addr);

    return true;
}

bool BinProg::programming_bin(const target_cfg_t &cfg, uint32_t addr)
{
    return programming_bin(cfg, addr, _file_path);
}