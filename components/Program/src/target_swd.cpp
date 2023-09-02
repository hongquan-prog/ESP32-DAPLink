#include "swd_host.h"
#include "target_swd.h"

TargetSWD &TargetSWD::get_instance()
{
    static TargetSWD instance;
    return instance;
}

uint8_t TargetSWD::init(void)
{
    return swd_init();
}

uint8_t TargetSWD::off(void)
{
    return swd_off();
}

uint8_t TargetSWD::init_debug(void)
{
    return swd_init_debug();
}

uint8_t TargetSWD::read_dp(uint8_t adr, uint32_t *val)
{
    return swd_read_dp(adr, val);
}

uint8_t TargetSWD::write_dp(uint8_t adr, uint32_t val)
{
    return swd_write_dp(adr, val);
}

uint8_t TargetSWD::read_ap(uint32_t adr, uint32_t *val)
{
    return swd_read_ap(adr, val);
}

uint8_t TargetSWD::write_ap(uint32_t adr, uint32_t val)
{
    return swd_write_ap(adr, val);
}

uint8_t TargetSWD::read_memory(uint32_t address, uint8_t *data, uint32_t size)
{
    return swd_read_memory(address, data, size);
}

uint8_t TargetSWD::write_memory(uint32_t address, uint8_t *data, uint32_t size)
{
    return swd_write_memory(address, data, size);
}

uint8_t TargetSWD::flash_syscall_exec(const SWDInface::syscall_t *syscall_param, uint32_t entry, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    return swd_flash_syscall_exec(reinterpret_cast<const ::program_syscall_t *>(syscall_param), entry, arg1, arg2, arg3, arg4);
}

void TargetSWD::set_target_reset(uint8_t asserted)
{
    swd_set_target_reset(asserted);
}

uint8_t TargetSWD::set_target_state_hw(target_state_t state)
{
    return swd_set_target_state_hw(static_cast<::target_state_t>(state));
}

uint8_t TargetSWD::set_target_state_sw(target_state_t state)
{
    return swd_set_target_state_sw(static_cast<::target_state_t>(state));
}
