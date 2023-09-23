#pragma once

#include "program_iface.h"
#include <functional>

class FileProgrammer
{
public:
    using progress_changed_cb_t = std::function<void(int progress)>;

private:
    ProgramIface &_binary_program;
    ProgramIface &_hex_program;
    int _program_progress;
    progress_changed_cb_t _progress_changed_cb;

    static constexpr int _buf_size = 256;
    uint8_t _buffer[_buf_size];

    ProgramIface *selcet_program_iface(const std::string &path);
    void set_program_progress(int progress);

public:
    FileProgrammer(ProgramIface &binary_program, ProgramIface &hex_program);
    bool program(const std::string &path, FlashIface::target_cfg_t &cfg, uint32_t program_addr = 0);
    int get_program_progress(void);
    void register_progress_changed_callback(const progress_changed_cb_t &func);
    static bool is_exist(const char *path);
    static bool compare_extension(const char *filename, const char *extension);
};
