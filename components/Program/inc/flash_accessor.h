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

#include "target_flash.h"
#include <cstdint>

/**
 * @brief Flash memory accessor with buffering and sector management
 * 
 * This class provides buffered flash writing with automatic sector erasing
 * and block management. It optimizes flash programming by minimizing
 * erase/write cycles.
 */
class FlashAccessor : public TargetFlash
{
private:
    static constexpr uint32_t _page_size = 1024;   ///< Page buffer size
    
    FlashIface::state_t _flash_state;              ///< Flash state machine state
    bool _current_sector_valid;                    ///< Current sector setup flag
    uint32_t _last_packet_addr;                    ///< Last packet address
    uint32_t _current_write_block_addr;            ///< Current write block start address
    uint32_t _current_write_block_size;            ///< Current write block size
    uint32_t _current_sector_addr;                 ///< Current sector start address
    uint32_t _current_sector_size;                 ///< Current sector size
    bool _page_buf_empty;                          ///< Page buffer empty flag
    uint8_t _page_buffer[_page_size];              ///< Page buffer for buffered writes

    /**
     * @brief Private constructor (Singleton pattern)
     */
    FlashAccessor();
    
    /**
     * @brief Flush current write block to flash
     * @param addr Target address
     * @return ERR_NONE on success
     */
    FlashIface::err_t flush_current_block(uint32_t addr);
    
    /**
     * @brief Setup next sector for writing
     * @param addr Target address
     * @return ERR_NONE on success
     */
    FlashIface::err_t setup_next_sector(uint32_t addr);

public:
    /**
     * @brief Destructor
     */
    ~FlashAccessor() = default;
    
    /**
     * @brief Get singleton instance
     * @return Reference to FlashAccessor instance
     */
    static FlashAccessor &get_instance();
    
    /**
     * @brief Initialize flash accessor
     * @param cfg Target flash configuration
     * @return ERR_NONE on success
     */
    FlashIface::err_t init(const target_cfg_t &cfg);
    
    /**
     * @brief Write data to flash with buffering
     * @param addr Target address
     * @param data Data buffer
     * @param size Data size
     * @return ERR_NONE on success
     */
    FlashIface::err_t write(uint32_t addr, const uint8_t *data, uint32_t size);
    
    /**
     * @brief Uninitialize flash accessor
     * @return ERR_NONE on success
     */
    FlashIface::err_t uninit();
};
