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

#include "programmer/prog_offline.h"
#include "stream_programmer.h"

/**
 * @file prog_online.h
 * @brief Online programming state
 * 
 * This state handles streaming programming operations.
 * It programs data received over the network in real-time.
 */

/**
 * @brief Online programming state class
 * 
 * Handles streaming programming where data is received in chunks.
 * Uses StreamProgrammer to program BIN or HEX data streams.
 */
class ProgOnline : public ProgOffline
{
private:
    bool _recved_new_packet;      ///< New packet received flag
    uint32_t _start_time;         ///< Programming start time
    uint32_t _writed_offset;      ///< Bytes written so far
    uint32_t _total_size;         ///< Total data size
    StreamProgrammer _stream_program;  ///< Stream programmer

public:
    /**
     * @brief Constructor
     */
    ProgOnline();
    
    /**
     * @brief Handle programming start
     * @param obj Programmer data object
     */
    virtual void program_start_handle(ProgData &obj) override;
    
    /**
     * @brief Handle programming timeout
     * @param obj Programmer data object
     */
    virtual void program_timeout_handle(ProgData &obj) override;
    
    /**
     * @brief Handle programming data received
     * @param obj Programmer data object
     */
    virtual void program_data_handle(ProgData &obj) override;
    
    /**
     * @brief Get state name
     * @return "prog_online"
     */
    virtual const char *name() override;
};