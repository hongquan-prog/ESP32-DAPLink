/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   Initial version
 * 2026-3-16     Refactor     Improved interface structure and documentation
 */
#pragma once

#include <cstdint>

/**
 * @brief SWD (Serial Wire Debug) interface for target device communication
 * 
 * This class provides low-level SWD/JTAG operations for communicating
 * with ARM Cortex-M target devices.
 */
class SWDIface
{
public:
    /**
     * @brief System call parameters for flash algorithm execution
     */
    typedef struct
    {
        uint32_t breakpoint;      ///< Breakpoint address
        uint32_t static_base;     ///< Static base pointer
        uint32_t stack_pointer;   ///< Stack pointer address
    } syscall_t;

    /**
     * @brief SWD transfer error codes
     */
    enum transfer_err_def
    {
        TRANSFER_OK = 0x01,
        TRANSFER_WAIT = 0x02,
        TRANSFER_FAULT = 0x04,
        TRANSFER_ERROR = 0x08,
        TRANSFER_MISMATCH = 0x10
    };

    /**
     * @brief Target device state control
     */
    typedef enum
    {
        TARGET_RESET_HOLD,       ///< Hold target in reset
        TARGET_RESET_PROGRAM,    ///< Reset target and setup for flash programming
        TARGET_RESET_RUN,        ///< Reset target and run normally
        TARGET_NO_DEBUG,         ///< Disable debug on running target
        TARGET_DEBUG,            ///< Enable debug on running target
        TARGET_HALT,             ///< Halt the target without resetting it
        TARGET_RUN,              ///< Resume the target without resetting it
        TARGET_POST_FLASH_RESET, ///< Reset target after flash programming
        TARGET_POWER_ON,         ///< Power on the target
        TARGET_SHUTDOWN,         ///< Power off the target
    } target_state_t;

    virtual ~SWDIface() = default;

    /**
     * @brief Delay in milliseconds
     * @param ms Number of milliseconds to delay
     */
    virtual void msleep(uint32_t ms) = 0;
    
    /**
     * @brief Initialize SWD interface
     * @return true if initialization successful
     */
    virtual bool init(void) = 0;
    
    /**
     * @brief Shutdown SWD interface
     * @return true if shutdown successful
     */
    virtual bool off(void) = 0;
    
    /**
     * @brief Perform SWD transfer
     * @param request Transfer request code
     * @param data Pointer to data buffer
     * @return Transfer status code
     */
    virtual transfer_err_def transer(uint32_t request, uint32_t *data) = 0;
    
    /**
     * @brief Execute SWJ sequence
     * @param count Number of bits to transfer
     * @param data Pointer to data buffer
     */
    virtual void swj_sequence(uint32_t count, const uint8_t *data) = 0;
    
    /**
     * @brief Set target reset state
     * @param asserted true to assert reset, false to release
     */
    virtual void set_target_reset(uint8_t asserted) = 0;

    // High-level helper methods with default implementations
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
