#include "esp_log.h"
#include "bin_programmer.h"

#define TAG "bin_programmer"

BinProgrammer::BinProgrammer()
    : _file(),
      _flash_manager(FlashManager::get_instance())
{
}

BinProgrammer::BinProgrammer(const std::string &file)
    : _file(file)
{
}

bool BinProgrammer::bin_write(uint32_t address, uint8_t *data, uint32_t size)
{
    return false;
}

bool BinProgrammer::start(target_cfg_t &cfg, uint32_t addr, const std::string &file)
{
    UINT line_size = 0;
    error_t err = ERROR_SUCCESS;
    FRESULT ret = FR_OK;

    err = _flash_manager.init(&cfg);
    if (err != ERROR_SUCCESS)
    {
        return false;
    }

    if (file.empty())
    {
        ESP_LOGE(TAG, "No file specified");
        return false;
    }

    ret = f_open(&_file_object, file.c_str(), FA_READ);
    if (ret != FR_OK)
    {
        ESP_LOGE(TAG, "Failed to open %s", file.c_str());
        return false;
    }

    while (f_eof(&_file_object) == 0)
    {
        f_read(&_file_object, _bin_buffer, sizeof(_bin_buffer), &line_size);

        if (line_size > 0)
        {
            if (ERROR_SUCCESS != _flash_manager.write(addr, _bin_buffer, line_size))
            {
                ESP_LOGE(TAG, "Failed to write bin at:%ld", addr);
                _flash_manager.uninit();
                return false;
            }

            addr += line_size;
        }
    }

    _flash_manager.uninit();
    return true;
}

bool BinProgrammer::start(target_cfg_t &cfg, uint32_t addr)
{
    return start(cfg, addr, _file);
}