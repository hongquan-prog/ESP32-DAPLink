#pragma once

#include "swd_iface.h"

class TargetSWD : public SWDInface
{
private:
    TargetSWD() = default;

public:
    static TargetSWD &get_instance();
    virtual uint8_t init(void) override;
    virtual uint8_t off(void) override;
    virtual uint8_t init_debug(void) override;
    virtual uint8_t read_dp(uint8_t adr, uint32_t *val) override;
    virtual uint8_t write_dp(uint8_t adr, uint32_t val) override;
    virtual uint8_t read_ap(uint32_t adr, uint32_t *val) override;
    virtual uint8_t write_ap(uint32_t adr, uint32_t val) override;
    virtual uint8_t read_memory(uint32_t address, uint8_t *data, uint32_t size) override;
    virtual uint8_t write_memory(uint32_t address, uint8_t *data, uint32_t size) override;
    virtual uint8_t flash_syscall_exec(const syscall_t *syscall_param, uint32_t entry, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4) override;
    virtual void set_target_reset(uint8_t asserted) override;
    virtual uint8_t set_target_state_hw(target_state_t state) override;
    virtual uint8_t set_target_state_sw(target_state_t state) override;
};
