#include "prog.h"

Prog::prog_swith_mode_t Prog::_switch_func = nullptr;

void Prog::register_switch_mode_function(const prog_swith_mode_t &func)
{
    _switch_func = func;
}

void Prog::switch_mode(prog_mode_def mode)
{
    if (_switch_func)
    {
        _switch_func(mode);
    }
}

void Prog::request_handle(ProgData &obj)
{
    obj.set_swap(reinterpret_cast<void *>(PROG_ERR_INVALID_OPERATION));
    obj.send_sync();
}

void Prog::program_start_handle(ProgData &obj)
{
}

void Prog::program_data_handle(ProgData &obj)
{
    obj.set_swap(reinterpret_cast<void *>(PROG_ERR_INVALID_OPERATION));
    obj.send_sync();
}

void Prog::program_timeout_handle(ProgData &obj)
{
    obj.disable_timeout_timer();
}

const char *Prog::name()
{
    return "Prog";
};