#pragma once

#include "swd_iface.h"

class TargetSWD : public SWDIface
{
private:
    TargetSWD() = default;

public:
    static TargetSWD &get_instance();
    virtual void msleep(uint32_t ms) override;
    virtual bool init(void) override;
    virtual bool off(void) override;
    virtual transfer_err_def transer(uint32_t request, uint32_t *data) override;
    virtual void swj_sequence(uint32_t count, const uint8_t *data) override;
    virtual void set_target_reset(uint8_t asserted) override;
};
