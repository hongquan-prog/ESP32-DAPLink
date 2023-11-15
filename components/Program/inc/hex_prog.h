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

#include "bin_prog.h"
#include "hex_parser.h"
#include "ff.h"

class HexProg : public BinProg
{
private:
    static constexpr int _hex_buf_size = 256;
    uint8_t _hex_buffer[_hex_buf_size];
    hex_parser_t _parser;
    uint32_t _start_address;

    bool write_hex(const uint8_t *data, uint32_t size, uint32_t &decode_size);

public:
    HexProg();
    HexProg(const std::string &file);
    bool programing_hex(const FlashIface::target_cfg_t &cfg, const std::string &file);
    bool programing_hex(const FlashIface::target_cfg_t &cfg);
};