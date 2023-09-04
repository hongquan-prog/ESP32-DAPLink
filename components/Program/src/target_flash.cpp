#include "target_flash.h"
#include "esp_log.h"
#include <cstring>

#define TAG "target_flash"
#define DEFAULT_PROGRAM_PAGE_MIN_SIZE (256u)

TargetFlash::TargetFlash()
    : _swd(nullptr),
      _flash_cfg(nullptr),
      _last_func_type(FLASH_FUNC_NOP),
      _current_flash_algo(nullptr),
      _flash_state(FLASH_STATE_CLOSED),
      _flash_start_addr(0),
      _default_flash_region(nullptr)
{
}

const FlashIface::program_target_t *TargetFlash::get_flash_algo(uint32_t addr)
{
    for (auto &flash_region : _flash_cfg->flash_regions)
    {
        if (addr >= flash_region.start && addr <= flash_region.end)
        {
            _flash_start_addr = flash_region.start;

            if (flash_region.flash_algo)
            {
                return flash_region.flash_algo;
            }
            else
            {
                return nullptr;
            }
        }
    }

    // could not find a flash algo for the region; use default
    if (_default_flash_region)
    {
        _flash_start_addr = _default_flash_region->start;
        return _default_flash_region->flash_algo;
    }

    return nullptr;
}

FlashIface::err_t TargetFlash::flash_func_start(FlashIface::func_t func_type)
{
    if (_last_func_type != func_type)
    {
        // Finish the currently active function.
        if (FLASH_FUNC_NOP != _last_func_type && (FLASH_FUNC_NOP == func_type) &&
            !_swd->flash_syscall_exec(&_current_flash_algo->sys_call_s, _current_flash_algo->uninit, _last_func_type, 0, 0, 0))
        {
            return ERR_UNINIT;
        }

        // Start a new function.
        if (FLASH_FUNC_NOP != func_type && (FLASH_FUNC_NOP == _last_func_type) &&
            !_swd->flash_syscall_exec(&_current_flash_algo->sys_call_s, _current_flash_algo->init, _flash_start_addr, 0, func_type, 0))
        {
            return ERR_INIT;
        }

        _last_func_type = func_type;
    }

    return ERR_NONE;
}

void TargetFlash::swd_init(SWDIface &swd)
{
    _swd = &swd;
}

FlashIface::err_t TargetFlash::flash_init(const target_cfg_t &cfg)
{
    _flash_cfg = &cfg;
    _last_func_type = FLASH_FUNC_NOP;
    _current_flash_algo = nullptr;

    if (!_swd->set_target_state(SWDIface::TARGET_RESET_PROGRAM))
    {
        return ERR_RESET;
    }

    // get default region
    for (auto &flash_region : _flash_cfg->flash_regions)
    {
        if (flash_region.flags & REIGION_DEFAULT)
        {
            _default_flash_region = &flash_region;
            break;
        }
    }

    _flash_state = FLASH_STATE_OPEN;
    return ERR_NONE;
}

FlashIface::err_t TargetFlash::flash_uninit(void)
{
    if (_flash_cfg)
    {
        err_t status = flash_func_start(FLASH_FUNC_NOP);
        if (status != ERR_NONE)
        {
            return status;
        }

        // Resume the target if configured to do so
        _swd->set_target_state(SWDIface::TARGET_RESET_RUN);

        // Check to see if anything needs to be done after programming.
        // This is usually a no-op for most targets.
        _swd->set_target_state(SWDIface::TARGET_POST_FLASH_RESET);

        _flash_state = FLASH_STATE_CLOSED;
        _swd->off();
        return ERR_NONE;
    }
    else
    {
        return ERR_FAILURE;
    }
}

FlashIface::err_t TargetFlash::flash_program_page(uint32_t addr, const uint8_t *buf, uint32_t size)
{
    uint32_t write_size = 0;
    uint32_t write_size_duplicated = 0;
    err_t status = ERR_NONE;
    const program_target_t *flash_algo = _current_flash_algo;

    if (_flash_cfg)
    {
        if (!flash_algo)
        {
            ESP_LOGE(TAG, "No flash algo");
            return ERR_ALGO_MISSING;
        }

        status = flash_func_start(FLASH_FUNC_PROGRAM);
        if (status != ERR_NONE)
        {
            ESP_LOGE(TAG, "Error starting flash function");
            return status;
        }

        while (size > 0)
        {
            write_size = (size <= flash_algo->program_buffer_size) ? (size) : (flash_algo->program_buffer_size);

            // Write page to buffer
            if (!_swd->write_memory(flash_algo->program_buffer, (uint8_t *)buf, write_size))
            {
                ESP_LOGE(TAG, "Error writing flash buffer");
                return ERR_ALGO_DATA_SEQ;
            }

            // Run flash programming
            if (!_swd->flash_syscall_exec(&flash_algo->sys_call_s, flash_algo->program_page, addr, write_size, flash_algo->program_buffer, 0))
            {
                ESP_LOGE(TAG, "flash_syscall_exec program page error");
                return ERR_WRITE;
            }

            // Verify data flashed if in automation mode
            if (flash_algo->verify != 0)
            {
                status = flash_func_start(FLASH_FUNC_VERIFY);
                if (status != ERR_NONE)
                {
                    return status;
                }

                if (!_swd->flash_syscall_exec(&flash_algo->sys_call_s, flash_algo->verify, addr, write_size, flash_algo->program_buffer, 0))
                {
                    return ERR_WRITE_VERIFY;
                }

                addr += write_size;
                buf += write_size;
                size -= write_size;
            }
            // Verify data flashed if verify function is not provided
            else
            {
                write_size_duplicated = write_size;

                while (write_size_duplicated > 0)
                {
                    uint32_t verify_size = (write_size_duplicated <= sizeof(_verify_buf)) ? (write_size_duplicated) : (sizeof(_verify_buf));

                    if (!_swd->read_memory(addr, _verify_buf, verify_size))
                    {
                        ESP_LOGE(TAG, "Error reading flash buffer");
                        return ERR_ALGO_DATA_SEQ;
                    }

                    if (memcmp(buf, _verify_buf, verify_size) != 0)
                    {
                        ESP_LOGE(TAG, "Verify error at addr 0x%08lx", addr);
                        return ERR_WRITE_VERIFY;
                    }

                    addr += verify_size;
                    buf += verify_size;
                    size -= verify_size;
                    write_size_duplicated -= verify_size;
                }
            }

            ESP_LOGD(TAG, "Write %ld bytes to 0x%08lx", write_size, addr - write_size);
        }

        return ERR_NONE;
    }
    else
    {
        return ERR_FAILURE;
    }
}

FlashIface::err_t TargetFlash::flash_erase_sector(uint32_t addr)
{
    err_t status = ERR_NONE;
    const program_target_t *flash = _current_flash_algo;

    if (_flash_cfg)
    {
        if (!flash)
        {
            return ERR_INTERNAL;
        }

        // Check to make sure the address is on a sector boundary
        if ((addr % flash_erase_sector_size(addr)) != 0)
        {
            return ERR_ERASE_SECTOR;
        }

        status = flash_func_start(FLASH_FUNC_ERASE);

        if (status != ERR_NONE)
        {
            return status;
        }

        if (!_swd->flash_syscall_exec(&flash->sys_call_s, flash->erase_sector, addr, 0, 0, 0))
        {
            return ERR_ERASE_SECTOR;
        }

        return ERR_NONE;
    }
    else
    {
        return ERR_FAILURE;
    }
}

FlashIface::err_t TargetFlash::flash_erase_chip(void)
{
    err_t status = ERR_NONE;
    const program_target_t *new_flash_algo = nullptr;

    if (_flash_cfg)
    {
        for (auto &flash_region : _flash_cfg->flash_regions)
        {
            new_flash_algo = get_flash_algo(flash_region.start);

            if ((new_flash_algo == nullptr))
            {
                continue;
            }

            status = flash_algo_set(flash_region.start);
            if (status != ERR_NONE)
            {
                return status;
            }

            status = flash_func_start(FLASH_FUNC_ERASE);
            if (status != ERR_NONE)
            {
                return status;
            }

            if (!_swd->flash_syscall_exec(&_current_flash_algo->sys_call_s, _current_flash_algo->erase_chip, 0, 0, 0, 0))
            {
                return ERR_ERASE_ALL;
            }
        }

        // Reset and re-initialize the target after the erase if required
        if (_flash_cfg->erase_reset)
        {
            status = flash_init(*_flash_cfg);
        }

        return status;
    }
    else
    {
        return ERR_FAILURE;
    }
}

uint32_t TargetFlash::flash_program_page_min_size(uint32_t addr)
{
    uint32_t erase_size = 0;
    uint32_t size = DEFAULT_PROGRAM_PAGE_MIN_SIZE;

    if (_flash_cfg)
    {
        erase_size = flash_erase_sector_size(addr);

        return (size <= erase_size) ? size : erase_size;
    }
    else
    {
        return 0;
    }
}

uint32_t TargetFlash::flash_erase_sector_size(uint32_t addr)
{
    if (_flash_cfg)
    {
        if (_flash_cfg->sector_info.size())
        {
            for (auto it = _flash_cfg->sector_info.crbegin(); it != _flash_cfg->sector_info.crend(); ++it)
            {
                if (addr >= it->start)
                {
                    return it->size;
                }
            }
        }

        return 0;
    }
    else
    {
        return 0;
    }
}

uint8_t TargetFlash::flash_busy(void)
{
    return (_flash_state == FLASH_STATE_OPEN);
}

FlashIface::err_t TargetFlash::flash_algo_set(uint32_t addr)
{
    const program_target_t *new_flash_algo = get_flash_algo(addr);

    if (!new_flash_algo)
    {
        return ERR_ALGO_MISSING;
    }
    if (_current_flash_algo != new_flash_algo)
    {
        // run uninit to last func
        err_t status = flash_func_start(FLASH_FUNC_NOP);
        if (status != ERR_NONE)
        {
            return status;
        }
        // Download flash programming algorithm to target
        if (!_swd->write_memory(new_flash_algo->algo_start, (uint8_t *)new_flash_algo->algo_blob, new_flash_algo->algo_size))
        {
            ESP_LOGE(TAG, "Error writing flash algo");
            return ERR_ALGO_DL;
        }

        ESP_LOGI(TAG, "Flash algo write success");
        _current_flash_algo = new_flash_algo;
    }
    return ERR_NONE;
}
