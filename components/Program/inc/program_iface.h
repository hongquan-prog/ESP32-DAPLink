#pragma once

#include "flash_iface.h"

class ProgramIface
{
public:
    virtual ~ProgramIface() = default;
    virtual bool init(const FlashIface::target_cfg_t &cfg, uint32_t program_start_addr = 0) = 0;
    virtual bool write(uint8_t *data, size_t len) = 0;
    virtual size_t get_program_address(void) = 0;
    virtual void clean(void) = 0;
};