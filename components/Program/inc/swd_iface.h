#pragma once

#include <cstdint>

class SWDInface
{
public:
    typedef struct
    {
        uint32_t breakpoint;
        uint32_t static_base;
        uint32_t stack_pointer;
    } syscall_t;

    typedef enum
    {
        TARGET_RESET_HOLD,       // Hold target in reset
        TARGET_RESET_PROGRAM,    // Reset target and setup for flash programming.
        TARGET_RESET_RUN,        // Reset target and run normally
        TARGET_NO_DEBUG,         // Disable debug on running target
        TARGET_DEBUG,            // Enable debug on running target
        TARGET_HALT,             // Halt the target without resetting it
        TARGET_RUN,              // Resume the target without resetting it
        TARGET_POST_FLASH_RESET, // Reset target after flash programming
        TARGET_POWER_ON,         // Poweron the target
        TARGET_SHUTDOWN,         // Poweroff the target
    } target_state_t;

    virtual ~SWDInface() = default;
    virtual uint8_t init(void) = 0;
    virtual uint8_t off(void) = 0;
    virtual uint8_t init_debug(void) = 0;
    virtual uint8_t read_dp(uint8_t adr, uint32_t *val) = 0;
    virtual uint8_t write_dp(uint8_t adr, uint32_t val) = 0;
    virtual uint8_t read_ap(uint32_t adr, uint32_t *val) = 0;
    virtual uint8_t write_ap(uint32_t adr, uint32_t val) = 0;
    virtual uint8_t read_memory(uint32_t address, uint8_t *data, uint32_t size) = 0;
    virtual uint8_t write_memory(uint32_t address, uint8_t *data, uint32_t size) = 0;
    virtual uint8_t flash_syscall_exec(const syscall_t *sysCallParam, uint32_t entry, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4) = 0;
    virtual void set_target_reset(uint8_t asserted) = 0;
    virtual uint8_t set_target_state_hw(target_state_t state) = 0;
    virtual uint8_t set_target_state_sw(target_state_t state) = 0;
};
