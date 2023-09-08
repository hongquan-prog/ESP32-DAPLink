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

#include <string>
#include "flash_accessor.h"
#include "ff.h"

class BinProg
{
protected:
    static constexpr uint32_t _bin_buf_size = 256;
    std::string _file_path;
    FlashAccessor &_flash_accessor;
    uint8_t _bin_buffer[_bin_buf_size];

public:
    BinProg();
    BinProg(const std::string &file);
    virtual ~BinProg() = default;
    bool programming_bin(const FlashIface::target_cfg_t &cfg, uint32_t addr, const std::string &file);
    bool programming_bin(const FlashIface::target_cfg_t &cfg, uint32_t addr);
};