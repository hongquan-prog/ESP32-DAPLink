#include "esp_log.h"
#include "bin_prog.h"
#include "target_swd.h"

#define TAG "bin_prog"

BinProg::BinProg()
    : _file_path(),
      _flash_accessor(FlashAccessor::get_instance())
{
    _flash_accessor.swd_init(TargetSWD::get_instance());
}

BinProg::BinProg(const std::string &file)
    : _file_path(file), _flash_accessor(FlashAccessor::get_instance())
{
    _flash_accessor.swd_init(TargetSWD::get_instance());
}

bool BinProg::programming_bin(const FlashIface::target_cfg_t &cfg, uint32_t addr, const std::string &file)
{
    size_t rd_size = 0;
    FRESULT ret = FR_OK;
    uint32_t wr_addr = addr;
    uint32_t total_size = 0;
    FILE *fp = nullptr;
    FlashIface::err_t err = FlashIface::ERR_NONE;

    err = _flash_accessor.init(cfg);
    if (err != FlashIface::ERR_NONE)
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
            if (FlashIface::ERR_NONE != _flash_accessor.write(wr_addr, _bin_buffer, rd_size))
            {
                ESP_LOGE(TAG, "Failed to write bin at:%lx", wr_addr);
                _flash_accessor.uninit();
                return false;
            }

            wr_addr += rd_size;
        }
    }

    fclose(fp);
    _flash_accessor.uninit();
    ESP_LOGI(TAG, "DAPLink write %ld bytes to %s at 0x%08lx successfully", total_size, cfg.device_name.c_str(), addr);

    return true;
}

bool BinProg::programming_bin(const FlashIface::target_cfg_t &cfg, uint32_t addr)
{
    return programming_bin(cfg, addr, _file_path);
}