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
#include "program_iface.h"
#include "ff.h"

class BinaryProgram : public ProgramIface
{
protected:
    FlashAccessor &_flash_accessor;
    uint32_t _program_addr;

public:
    BinaryProgram();
    virtual ~BinaryProgram();
    virtual bool init(const FlashIface::target_cfg_t &cfg, uint32_t program_start_addr = 0) override;
    virtual bool write(uint8_t *data, size_t len) override;
    virtual size_t get_program_address(void) override;
    virtual void clean(void) override;
};