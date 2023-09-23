#pragma once

#include "program_iface.h"
#include <functional>

class StreamProgrammer
{
public:
    enum Mode
    {
        BIN_MODE,
        HEX_MODE
    };

private:
    ProgramIface &_binary_program;
    ProgramIface &_hex_program;
    ProgramIface *_iface;

public:
    StreamProgrammer(ProgramIface &binary_program, ProgramIface &hex_program);
    ~StreamProgrammer();
    bool init(StreamProgrammer::Mode mode, FlashIface::target_cfg_t &cfg, uint32_t program_addr = 0);
    bool write(uint8_t *data, size_t len);
    void clean(void);
};
