/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   Initial version
 * 2026-3-16     Refactor     Improved documentation
 */
#pragma once

#include "prog.h"

/**
 * @file prog_idle.h
 * @brief Idle state for programmer state machine
 * 
 * This state handles initial programming requests and switches
 * to the appropriate programming mode.
 */

/**
 * @brief Idle state class
 * 
 * Handles initial requests and switches to Online or Offline mode
 * based on the request parameters.
 */
class ProgIdle : public Prog
{
public:
    /**
     * @brief Handle programming request
     * @param obj Programmer data object
     */
    virtual void request_handle(ProgData &obj) override;
    
    /**
     * @brief Get state name
     * @return "prog_idle"
     */
    virtual const char *name() override;
};
