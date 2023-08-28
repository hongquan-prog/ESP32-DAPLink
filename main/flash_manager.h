#pragma once

#include "error.h"
#include "flash_intf.h"

class FlashManager
{
private:
    typedef enum
    {
        FLASH_STATE_CLOSED,
        FLASH_STATE_OPEN,
        FLASH_STATE_ERROR
    } flash_state_t;

    static constexpr uint32_t _sector_size = 1024;
    flash_intf_t *_flash_intf;
    flash_state_t _flash_state;
    bool _current_sector_valid;
    uint32_t _last_packet_addr = 0;
    uint32_t _current_write_block_addr;
    uint32_t _current_write_block_size;
    uint32_t _current_sector_addr;
    uint32_t _current_sector_size;
    bool _page_buf_empty;
    uint8_t _page_buffer[_sector_size];

    error_t flush_current_block(uint32_t addr);
    error_t setup_next_sector(uint32_t addr);
    bool flash_intf_valid(flash_intf_t *flash_intf);

public:
    FlashManager();
    ~FlashManager() = default;
    error_t init(flash_intf_t *flash_intf);
    error_t write(uint32_t addr, const uint8_t *data, uint32_t size);
    error_t uninit();
};
