/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   add license declaration
 */
#pragma once

#include <cstdint>
#include "flash_iface.h"

class TargetFlash : public FlashIface
{
private:
    SWDIface *_swd;
    const target_cfg_t *_flash_cfg;
    FlashIface::func_t _last_func_type;
    const program_target_t *_current_flash_algo;
    FlashIface::state_t _flash_state;
    uint32_t _flash_start_addr;
    const region_info_t *_default_flash_region;
    uint8_t _verify_buf[256];

    err_t flash_func_start(FlashIface::func_t func);
    const FlashIface::program_target_t *get_flash_algo(uint32_t addr);

public:
    TargetFlash();
    virtual void swd_init(SWDIface &swd) override;
    virtual err_t flash_init(const target_cfg_t &cfg) override;
    virtual err_t flash_uninit(void) override;
    virtual err_t flash_program_page(uint32_t adr, const uint8_t *buf, uint32_t size) override;
    virtual err_t flash_erase_sector(uint32_t addr) override;
    virtual err_t flash_erase_chip(void) override;
    virtual uint32_t flash_program_page_min_size(uint32_t addr) override;
    virtual uint32_t flash_erase_sector_size(uint32_t addr) override;
    virtual uint8_t flash_busy(void) override;
    virtual err_t flash_algo_set(uint32_t addr) override;
};