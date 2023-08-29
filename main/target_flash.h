#pragma once

#include <cstdint>
#include "error.h"
#include "flash.h"

class TargetFlash : public Flash
{
private:
    target_cfg_t *_flash_cfg;
    flash_func_t _last_func_type;
    program_target_t *_current_flash_algo;
    flash_state_t _flash_state;
    uint32_t _flash_start_addr;
    region_info_t *_default_flash_region;

    error_t flash_func_start(flash_func_t func);
    program_target_t *get_flash_algo(uint32_t addr);

public:
    TargetFlash();
    virtual error_t flash_init(target_cfg_t *cfg) override;
    virtual error_t flash_uninit(void) override;
    virtual error_t flash_program_page(uint32_t adr, const uint8_t *buf, uint32_t size) override;
    virtual error_t flash_erase_sector(uint32_t addr) override;
    virtual error_t flash_erase_chip(void) override;
    virtual uint32_t flash_program_page_min_size(uint32_t addr) override;
    virtual uint32_t flash_erase_sector_size(uint32_t addr) override;
    virtual uint8_t flash_busy(void) override;
    virtual error_t flash_algo_set(uint32_t addr) override;
};