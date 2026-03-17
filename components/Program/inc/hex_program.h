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

#include "bin_program.h"
#include "hex_parser.h"
#include "ff.h"

/**
 * @brief HEX file flash programmer
 * 
 * This class handles programming flash memory from Intel HEX format data.
 * It parses HEX records and writes the decoded binary data to flash.
 */
class HexProgram : public BinaryProgram
{
private:
    static constexpr int _decode_buf_size = 256;   ///< Decode buffer size
    uint8_t _decode_buffer[_decode_buf_size];      ///< Buffer for decoded data
    hex_parser_t _hex_parser;                      ///< HEX parser state

    /**
     * @brief Write HEX data to flash
     * @param data Pointer to HEX data buffer
     * @param size Length of HEX data
     * @param decode_size Output: decoded binary size
     * @return true if write successful
     */
    bool write_hex(const uint8_t *data, uint32_t size, uint32_t &decode_size);

public:
    /**
     * @brief Constructor
     */
    HexProgram();
    
    /**
     * @brief Initialize programmer with target configuration
     * @param cfg Target flash configuration
     * @param program_addr Start address for programming
     * @return true if initialization successful
     */
    virtual bool init(const FlashIface::target_cfg_t &cfg, uint32_t program_addr) override;
    
    /**
     * @brief Write HEX data to flash
     * @param data Pointer to data buffer
     * @param len Length of data to write
     * @return true if write successful
     */
    virtual bool write(uint8_t *data, size_t len) override;
};