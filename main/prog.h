#pragma once

#include "prog_data.h"
#include <functional>

class Prog
{
public:
    using prog_swith_mode_t = std::function<void(prog_mode_def)>;

private:
    static prog_swith_mode_t _switch_func;

public:
    virtual ~Prog() = default;
    virtual void request_handle(ProgData &obj);
    virtual void program_start_handle(ProgData &obj);
    virtual void program_timeout_handle(ProgData &obj);
    virtual void program_data_handle(ProgData &obj);
    virtual const char *name();
    static void register_switch_mode_function(const prog_swith_mode_t &func);
    static void switch_mode(prog_mode_def mode);
};