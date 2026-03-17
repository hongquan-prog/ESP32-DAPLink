/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   Initial version
 * 2026-3-16     Refactor     Improved documentation and structure
 */
#pragma once

#include "programmer/prog_data.h"
#include <functional>

/**
 * @file prog.h
 * @brief Base class for programmer state machine
 * 
 * This file defines the base class for all programmer states (Idle, Online, Offline).
 * Each state handles specific programming operations.
 */

/**
 * @brief Base class for programmer state machine
 * 
 * This class uses the State pattern to handle different programming modes.
 * Subclasses implement specific behavior for each state.
 */
class Prog
{
public:
    /**
     * @brief Function type for switching mode
     * @param mode Target mode
     */
    using prog_switch_mode_t = std::function<void(prog_mode_def)>;

private:
    static prog_switch_mode_t _switch_func;  ///< Mode switch function

public:
    /**
     * @brief Virtual destructor
     */
    virtual ~Prog() = default;
    
    /**
     * @brief Handle programming request
     * @param obj Programmer data object
     */
    virtual void request_handle(ProgData &obj);
    
    /**
     * @brief Handle programming start
     * @param obj Programmer data object
     */
    virtual void program_start_handle(ProgData &obj);
    
    /**
     * @brief Handle programming timeout
     * @param obj Programmer data object
     */
    virtual void program_timeout_handle(ProgData &obj);
    
    /**
     * @brief Handle programming data received
     * @param obj Programmer data object
     */
    virtual void program_data_handle(ProgData &obj);
    
    /**
     * @brief Get state name
     * @return State name string
     */
    virtual const char *name();
    
    /**
     * @brief Register mode switch function
     * @param func Switch function
     */
    static void register_switch_mode_function(const prog_switch_mode_t &func);
    
    /**
     * @brief Switch to specified mode
     * @param mode Target mode
     */
    static void switch_mode(prog_mode_def mode);
};