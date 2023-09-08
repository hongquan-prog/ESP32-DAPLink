/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   add license declaration
 */
#pragma once

#include <cstdint>

class SWDIface
{
public:
    typedef struct
    {
        uint32_t breakpoint;
        uint32_t static_base;
        uint32_t stack_pointer;
    } syscall_t;

    enum transfer_err_def
    {
        TRANSFER_OK = 0x01,
        TRANSFER_WAIT = 0x02,
        TRANSFER_FAULT = 0x04,
        TRANSFER_ERROR = 0x08,
        TRANSFER_MISMATCH = 0x10
    };

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

    virtual ~SWDIface() = default;

    virtual void msleep(uint32_t ms) = 0;
    virtual bool init(void) = 0;
    virtual bool off(void) = 0;
    virtual transfer_err_def transer(uint32_t request, uint32_t *data) = 0;
    virtual void swj_sequence(uint32_t count, const uint8_t *data) = 0;
    virtual void set_target_reset(uint8_t asserted) = 0;

    bool init_debug(void);
    transfer_err_def transfer_retry(uint32_t req, uint32_t *data);
    bool read_dp(uint8_t adr, uint32_t *val);
    bool write_dp(uint8_t adr, uint32_t val);
    bool read_ap(uint32_t adr, uint32_t *val);
    bool write_ap(uint32_t adr, uint32_t val);
    bool set_target_state(target_state_t state);
    bool read_memory(uint32_t address, uint8_t *data, uint32_t size);
    bool write_memory(uint32_t address, uint8_t *data, uint32_t size);
    bool flash_syscall_exec(const syscall_t *sysCallParam, uint32_t entry, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);

protected:
    typedef struct
    {
        uint32_t select;
        uint32_t csw;
    } dap_state_t;

    typedef struct
    {
        uint32_t r[16];
        uint32_t xpsr;
    } debug_state_t;

    dap_state_t _dap_state;

    bool write_block(uint32_t address, uint8_t *data, uint32_t size);
    bool read_block(uint32_t address, uint8_t *data, uint32_t size);
    bool read_data(uint32_t addr, uint32_t *val);
    bool write_data(uint32_t address, uint32_t data);
    bool read_word(uint32_t addr, uint32_t *val);
    bool write_word(uint32_t addr, uint32_t val);
    bool read_byte(uint32_t addr, uint8_t *val);
    bool write_byte(uint32_t addr, uint8_t val);
    bool read_core_register(uint32_t n, uint32_t *val);
    bool write_core_register(uint32_t n, uint32_t val);
    bool write_debug_state(debug_state_t *state);
    bool wait_until_halted(void);
    bool swd_reset(void);
    bool swd_switch(uint16_t val);
    bool read_idcode(uint32_t *id);
    bool jtag_to_swd(void);
};
