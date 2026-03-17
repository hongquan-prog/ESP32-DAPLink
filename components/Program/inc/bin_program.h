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

#include <string>
#include <cstdint>
#include "flash_accessor.h"
#include "program_iface.h"
#include "ff.h"

/**
 * @brief Binary file flash programmer
 * 
 * This class handles programming flash memory from binary data.
 * It writes data directly to flash at specified addresses.
 */
class BinaryProgram : public ProgramIface
{
protected:
    FlashAccessor &_flash_accessor;   ///< Flash accessor instance
    uint32_t _program_addr;           ///< Current programming address

public:
    /**
     * @brief Constructor
     */
    BinaryProgram();
    
    /**
     * @brief Destructor
     */
    virtual ~BinaryProgram();
    
    /**
     * @brief Initialize programmer with target configuration
     * @param cfg Target flash configuration
     * @param program_start_addr Start address for programming
     * @return true if initialization successful
     */
    virtual bool init(const FlashIface::target_cfg_t &cfg, uint32_t program_start_addr = 0) override;
    
    /**
     * @brief Write binary data to flash
     * @param data Pointer to data buffer
     * @param len Length of data to write
     * @return true if write successful
     */
    virtual bool write(uint8_t *data, size_t len) override;
    
    /**
     * @brief Get current programming address
     * @return Current address being programmed
     */
    virtual size_t get_program_address(void) override;
    
    /**
     * @brief Clean up and reset programmer state
     */
    virtual void clean(void) override;
};