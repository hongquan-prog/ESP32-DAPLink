/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   Initial version
 * 2026-3-16     Refactor     Improved interface documentation
 */
#pragma once

#include <cstdint>
#include "flash_iface.h"

/**
 * @brief Target flash implementation using flash algorithms
 * 
 * This class implements the FlashIface interface using Keil-style
 * flash algorithms. It manages flash algorithm loading and execution.
 */
class TargetFlash : public FlashIface
{
private:
    SWDIface *_swd;                        ///< SWD interface instance
    const target_cfg_t *_flash_cfg;        ///< Flash configuration
    FlashIface::func_t _last_func_type;    ///< Last flash function executed
    const program_target_t *_current_flash_algo;  ///< Current flash algorithm
    FlashIface::state_t _flash_state;      ///< Flash state machine state
    uint32_t _flash_start_addr;            ///< Flash start address
    const region_info_t *_default_flash_region;  ///< Default flash region
    uint8_t _verify_buf[256];              ///< Buffer for verify operations

    /**
     * @brief Start flash function execution
     * @param func Function to execute
     * @return ERR_NONE on success
     */
    err_t flash_func_start(FlashIface::func_t func);
    
    /**
     * @brief Get flash algorithm for address
     * @param addr Target address
     * @return Pointer to flash algorithm structure
     */
    const FlashIface::program_target_t *get_flash_algo(uint32_t addr);

public:
    /**
     * @brief Constructor
     */
    TargetFlash();
    
    /**
     * @brief Initialize SWD interface
     * @param swd SWD interface instance
     */
    virtual void swd_init(SWDIface &swd) override;
    
    /**
     * @brief Initialize flash interface
     * @param cfg Target flash configuration
     * @return ERR_NONE on success
     */
    virtual err_t flash_init(const target_cfg_t &cfg) override;
    
    /**
     * @brief Uninitialize flash interface
     * @return ERR_NONE on success
     */
    virtual err_t flash_uninit(void) override;
    
    /**
     * @brief Program a page in flash
     * @param adr Destination address
     * @param buf Source buffer
     * @param size Number of bytes
     * @return ERR_NONE on success
     */
    virtual err_t flash_program_page(uint32_t adr, const uint8_t *buf, uint32_t size) override;
    
    /**
     * @brief Erase a flash sector
     * @param addr Sector address
     * @return ERR_NONE on success
     */
    virtual err_t flash_erase_sector(uint32_t addr) override;
    
    /**
     * @brief Erase entire flash chip
     * @return ERR_NONE on success
     */
    virtual err_t flash_erase_chip(void) override;
    
    /**
     * @brief Get minimum program page size
     * @param addr Address to check
     * @return Minimum page size
     */
    virtual uint32_t flash_program_page_min_size(uint32_t addr) override;
    
    /**
     * @brief Get flash sector size
     * @param addr Address to check
     * @return Sector size
     */
    virtual uint32_t flash_erase_sector_size(uint32_t addr) override;
    
    /**
     * @brief Check if flash is busy
     * @return true if busy
     */
    virtual uint8_t flash_busy(void) override;
    
    /**
     * @brief Set flash algorithm
     * @param addr Algorithm address
     * @return ERR_NONE on success
     */
    virtual err_t flash_algo_set(uint32_t addr) override;
};