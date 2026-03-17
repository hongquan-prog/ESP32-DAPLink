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

#include "program_iface.h"
#include <functional>

/**
 * @brief Stream-based flash programmer
 * 
 * This class handles programming flash from streaming data (BIN or HEX format).
 * It's useful for programming data received over a network or serial connection.
 */
class StreamProgrammer
{
public:
    /**
     * @brief Programming mode selection
     */
    enum Mode
    {
        BIN_MODE,  ///< Binary mode
        HEX_MODE   ///< HEX file mode
    };

private:
    ProgramIface &_binary_program;  ///< Binary programmer instance
    ProgramIface &_hex_program;     ///< HEX programmer instance
    ProgramIface *_iface;           ///< Currently selected programmer

public:
    /**
     * @brief Constructor
     * @param binary_program Binary programmer instance
     * @param hex_program HEX programmer instance
     */
    StreamProgrammer(ProgramIface &binary_program, ProgramIface &hex_program);
    
    /**
     * @brief Destructor
     */
    ~StreamProgrammer();
    
    /**
     * @brief Initialize stream programmer
     * @param mode Programming mode (BIN or HEX)
     * @param cfg Target flash configuration
     * @param program_addr Optional start address for programming
     * @return true if initialization successful
     */
    bool init(StreamProgrammer::Mode mode, FlashIface::target_cfg_t &cfg, uint32_t program_addr = 0);
    
    /**
     * @brief Write data to flash
     * @param data Pointer to data buffer
     * @param len Length of data to write
     * @return true if write successful
     */
    bool write(uint8_t *data, size_t len);
    
    /**
     * @brief Clean up and reset programmer state
     */
    void clean(void);
};
