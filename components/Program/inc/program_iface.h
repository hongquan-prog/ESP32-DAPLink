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

#include "flash_iface.h"
#include <cstdint>
#include <cstddef>

/**
 * @brief Abstract interface for flash programming operations
 * 
 * This interface provides a unified API for programming flash memory
 * using different file formats (BIN, HEX, etc.)
 */
class ProgramIface
{
public:
    virtual ~ProgramIface() = default;
    
    /**
     * @brief Initialize the programmer with target configuration
     * @param cfg Target flash configuration
     * @param program_start_addr Start address for programming (optional)
     * @return true if initialization successful
     */
    virtual bool init(const FlashIface::target_cfg_t &cfg, uint32_t program_start_addr = 0) = 0;
    
    /**
     * @brief Write data to flash
     * @param data Pointer to data buffer
     * @param len Length of data to write
     * @return true if write successful
     */
    virtual bool write(uint8_t *data, size_t len) = 0;
    
    /**
     * @brief Get current programming address
     * @return Current address being programmed
     */
    virtual size_t get_program_address(void) = 0;
    
    /**
     * @brief Clean up and reset programmer state
     */
    virtual void clean(void) = 0;
};