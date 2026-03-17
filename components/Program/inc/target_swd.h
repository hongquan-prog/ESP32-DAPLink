/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   Initial version
 * 2026-3-16     Refactor     Improved interface documentation
 */
#pragma once

#include "swd_iface.h"

/**
 * @brief Target SWD implementation for ESP32
 * 
 * This class implements the SWDIface interface using ESP32's
 * USB-JTAG controller or bit-banged SWD.
 */
class TargetSWD : public SWDIface
{
private:
    /**
     * @brief Private constructor (Singleton pattern)
     */
    TargetSWD() = default;

public:
    /**
     * @brief Get singleton instance
     * @return Reference to TargetSWD instance
     */
    static TargetSWD &get_instance();
    
    /**
     * @brief Delay in milliseconds
     * @param ms Number of milliseconds
     */
    virtual void msleep(uint32_t ms) override;
    
    /**
     * @brief Initialize SWD interface
     * @return true if successful
     */
    virtual bool init(void) override;
    
    /**
     * @brief Shutdown SWD interface
     * @return true if successful
     */
    virtual bool off(void) override;
    
    /**
     * @brief Perform SWD transfer
     * @param request Transfer request
     * @param data Data buffer
     * @return Transfer status
     */
    virtual transfer_err_def transer(uint32_t request, uint32_t *data) override;
    
    /**
     * @brief Execute SWJ sequence
     * @param count Number of bits
     * @param data Data buffer
     */
    virtual void swj_sequence(uint32_t count, const uint8_t *data) override;
    
    /**
     * @brief Set target reset state
     * @param asserted Reset state (true = assert)
     */
    virtual void set_target_reset(uint8_t asserted) override;
};
