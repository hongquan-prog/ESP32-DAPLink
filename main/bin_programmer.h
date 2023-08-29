#pragma once

#include <string>
#include "flash_manager.h"
#include "ff.h"

class BinProgrammer
{
protected:
    static constexpr uint32_t _flash_sector_size = 512;
    std::string _file;
    FlashManager &_flash_manager;
    FIL _file_object;
    uint8_t _bin_buffer[_flash_sector_size];

public:
    BinProgrammer();
    BinProgrammer(const std::string &file);
    virtual ~BinProgrammer();
    bool bin_write(uint32_t address, uint8_t *data, uint32_t size);
    bool start(target_cfg_t &cfg, uint32_t addr, const std::string &file);
    bool start(target_cfg_t &cfg, uint32_t addr);
};