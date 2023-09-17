/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   add license declaration
 */
#include "log.h"
#include "bin_program.h"
#include "target_swd.h"

#define TAG "bin_prog"

BinaryProgram::BinaryProgram()
    : _flash_accessor(FlashAccessor::get_instance()),
      _program_addr(0)
{
    _flash_accessor.swd_init(TargetSWD::get_instance());
}

BinaryProgram::~BinaryProgram()
{
    _flash_accessor.uninit();
}

bool BinaryProgram::init(const FlashIface::target_cfg_t &cfg, uint32_t program_addr)
{
    if (!program_addr)
    {
        LOG_ERROR("The binary file must be provided with program address, %ld", _program_addr);
        return false;
    }

    _program_addr = program_addr;

    return (_flash_accessor.init(cfg) == FlashIface::ERR_NONE);
}

bool BinaryProgram::write(uint8_t *data, size_t len)
{
    if (FlashIface::ERR_NONE != _flash_accessor.write(_program_addr, data, len))
    {
        LOG_ERROR("Failed to write data at:%lx", _program_addr);
        return false;
    }

    _program_addr += len;

    return true;
}

size_t BinaryProgram::get_program_address(void)
{
    return _program_addr;
}

void BinaryProgram::clean()
{
    _program_addr = 0;
    _flash_accessor.uninit();
}