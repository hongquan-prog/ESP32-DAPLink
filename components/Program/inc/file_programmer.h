#pragma once

#include "program_iface.h"

class FileProgrammer
{
private:
    static constexpr int _buf_size = 256;
    ProgramIface &_binary_program;
    ProgramIface &_hex_program;
    int _program_progress;
    uint8_t _buffer[_buf_size];

    ProgramIface *selcet_program_iface(const std::string &path);

public:
    FileProgrammer(ProgramIface &binary_program, ProgramIface &hex_program);
    void reset(void);
    bool program(const std::string &path, FlashIface::target_cfg_t &cfg, uint32_t program_addr = 0);
    int get_program_progress(void);
    static bool is_exist(const char *path);
    static bool compare_extension(const char *filename, const char *extension);
};
