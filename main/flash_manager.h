#pragma once

#include "error.h"
#include "target_flash.h"

class FlashManager : public TargetFlash
{
private:
    static constexpr uint32_t _page_size = 1024;
    flash_state_t _flash_state;
    bool _current_sector_valid;
    uint32_t _last_packet_addr = 0;
    uint32_t _current_write_block_addr;
    uint32_t _current_write_block_size;
    uint32_t _current_sector_addr;
    uint32_t _current_sector_size;
    bool _page_buf_empty;
    uint8_t _page_buffer[_page_size];

    FlashManager();
    dap_err_t flush_current_block(uint32_t addr);
    dap_err_t setup_next_sector(uint32_t addr);

public:
    ~FlashManager() = default;
    static FlashManager &get_instance();
    dap_err_t init(const target_cfg_t *cfg);
    dap_err_t write(uint32_t addr, const uint8_t *data, uint32_t size);
    dap_err_t uninit();
};
