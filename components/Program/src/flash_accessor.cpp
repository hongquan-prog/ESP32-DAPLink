#include "log.h"
#include "flash_accessor.h"
#include <cstring>

#define TAG "flash_accessor"
#define ROUND_UP(value, boundary) ((value) + ((boundary) - (value)) % (boundary))
#define ROUND_DOWN(value, boundary) ((value) - ((value) % (boundary)))

FlashAccessor::FlashAccessor()
    : TargetFlash(),
      _flash_state(FLASH_STATE_CLOSED),
      _current_sector_valid(false),
      _last_packet_addr(0),
      _current_write_block_addr(0),
      _current_write_block_size(0),
      _current_sector_addr(0),
      _current_sector_size(0),
      _page_buf_empty(true)
{
    memset(_page_buffer, 0xff, sizeof(_page_buffer));
}

FlashAccessor &FlashAccessor::get_instance()
{
    static FlashAccessor instance;
    return instance;
}

FlashIface::err_t FlashAccessor::flush_current_block(uint32_t addr)
{
    FlashIface::err_t status = ERR_NONE;

    // Write out current buffer if there is data in it
    if (!_page_buf_empty)
    {
        status = flash_program_page(_current_write_block_addr, _page_buffer, _current_write_block_size);
        _page_buf_empty = true;
    }

    // Setup for next block
    memset(_page_buffer, 0xFF, _current_write_block_size);

    if (!_current_write_block_size)
    {
        
        return ERR_INTERNAL;
    }

    _current_write_block_addr = ROUND_DOWN(addr, _current_write_block_size);

    return status;
}

FlashIface::err_t FlashAccessor::setup_next_sector(uint32_t addr)
{
    uint32_t min_prog_size = 0;
    uint32_t sector_size = 0;
    FlashIface::err_t status = ERR_NONE;

    min_prog_size = flash_program_page_min_size(addr);
    sector_size = flash_erase_sector_size(addr);

    if ((min_prog_size <= 0) || (sector_size <= 0))
    {
        return ERR_INTERNAL;
    }

    // Setup global variables
    _current_sector_addr = ROUND_DOWN(addr, sector_size);
    _current_sector_size = sector_size;
    _current_write_block_addr = _current_sector_addr;
    _current_write_block_size = (sector_size <= sizeof(_page_buffer)) ? (sector_size) : (sizeof(_page_buffer));

    // check flash algo every sector change, addresses with different flash algo should be sector aligned
    status = flash_algo_set(_current_sector_addr);
    if (ERR_NONE != status)
    {
        flash_uninit();
        return status;
    }

    // Erase the current sector
    status = flash_erase_sector(_current_sector_addr);
    if (ERR_NONE != status)
    {
        LOG_ERROR("Flash sector erase failed");
        flash_uninit();
        return status;
    }

    // Clear out buffer in case block size changed
    memset(_page_buffer, 0xFF, _current_write_block_size);

    return ERR_NONE;
}

FlashIface::err_t FlashAccessor::init(const target_cfg_t &cfg)
{
    FlashIface::err_t status = ERR_NONE;

    if (_flash_state != FLASH_STATE_CLOSED)
    {
        return ERR_INTERNAL;
    }

    // Initialize variables
    memset(_page_buffer, 0xFF, sizeof(_page_buffer));
    _page_buf_empty = true;
    _current_sector_valid = false;
    _current_write_block_addr = 0;
    _current_write_block_size = 0;
    _current_sector_addr = 0;
    _current_sector_size = 0;
    _last_packet_addr = 0;

    // Initialize flash
    status = flash_init(cfg);
    if (ERR_NONE != status)
    {
        LOG_ERROR("Flash init failed");
        return status;
    }

    LOG_INFO("Flash init successful");
    _flash_state = FLASH_STATE_OPEN;

    return status;
}

FlashIface::err_t FlashAccessor::write(uint32_t packet_addr, const uint8_t *data, uint32_t size)
{
    uint32_t page_buf_left = 0;
    uint32_t copy_size = 0;
    uint32_t copy_start_pos = 0;
    FlashIface::err_t status = ERR_NONE;

    if (_flash_state != FLASH_STATE_OPEN)
    {
        return ERR_INTERNAL;
    }

    // Setup the current sector if it is not setup already
    if (!_current_sector_valid)
    {
        status = setup_next_sector(packet_addr);

        if (ERR_NONE != status)
        {
            _flash_state = FLASH_STATE_ERROR;
            return status;
        }
        _current_sector_valid = true;
        _last_packet_addr = packet_addr;
    }

    // non-increasing address support
    if (ROUND_DOWN(packet_addr, _current_write_block_size) != ROUND_DOWN(_last_packet_addr, _current_write_block_size))
    {
        status = flush_current_block(packet_addr);
        if (ERR_NONE != status)
        {
            _flash_state = FLASH_STATE_ERROR;
            return status;
        }
    }

    if (ROUND_DOWN(packet_addr, _current_sector_size) != ROUND_DOWN(_last_packet_addr, _current_sector_size))
    {
        status = setup_next_sector(packet_addr);
        if (ERR_NONE != status)
        {
            _flash_state = FLASH_STATE_ERROR;
            return status;
        }
    }

    while (true)
    {
        // flush if necessary
        if (packet_addr >= _current_write_block_addr + _current_write_block_size)
        {
            status = flush_current_block(packet_addr);

            if (ERR_NONE != status)
            {
                _flash_state = FLASH_STATE_ERROR;
                return status;
            }
        }

        // Check for end
        if (size <= 0)
        {
            break;
        }

        // Change sector if necessary
        if (packet_addr >= _current_sector_addr + _current_sector_size)
        {
            status = setup_next_sector(packet_addr);

            if (ERR_NONE != status)
            {
                _flash_state = FLASH_STATE_ERROR;
                return status;
            }
        }

        // write buffer
        copy_start_pos = packet_addr - _current_write_block_addr;
        page_buf_left = _current_write_block_size - copy_start_pos;
        copy_size = ((size) < (page_buf_left) ? (size) : (page_buf_left));
        memcpy(_page_buffer + copy_start_pos, data, copy_size);
        _page_buf_empty = (copy_size == 0);

        // Update variables
        packet_addr += copy_size;
        data += copy_size;
        size -= copy_size;
    }

    _last_packet_addr = packet_addr;

    return status;
}

FlashIface::err_t FlashAccessor::uninit()
{
    FlashIface::err_t flash_write_ret = ERR_NONE;
    FlashIface::err_t flash_uninit_ret = ERR_NONE;

    if (FLASH_STATE_CLOSED == _flash_state)
    {
        return ERR_INTERNAL;
    }

    // Flush last buffer if its not empty
    if (FLASH_STATE_OPEN == _flash_state)
    {
        flash_write_ret = flush_current_block(0);
    }

    // Close flash interface (even if there was an error during program_page)
    flash_uninit_ret = flash_uninit();

    // Reset variables to catch accidental use
    memset(_page_buffer, 0xFF, sizeof(_page_buffer));

    _page_buf_empty = true;
    _current_sector_valid = false;
    _current_write_block_addr = 0;
    _current_write_block_size = 0;
    _current_sector_addr = 0;
    _current_sector_size = 0;
    _last_packet_addr = 0;
    _flash_state = FLASH_STATE_CLOSED;

    // Make sure an error from a page write or from an uninit gets propagated
    if (flash_uninit_ret != ERR_NONE)
    {
        return flash_uninit_ret;
    }

    if (flash_write_ret != ERR_NONE)
    {
        return flash_write_ret;
    }

    return ERR_NONE;
}