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
#include "bin_program.h"
#include "hex_program.h"
#include "file_programmer.h"

/**
 * @file prog_offline.h
 * @brief Offline programming state
 * 
 * This state handles file-based programming operations.
 * It programs files from the filesystem.
 */

/**
 * @brief Offline programming state class
 * 
 * Handles programming from files stored in the filesystem.
 * Uses FileProgrammer to program BIN or HEX files.
 */
class ProgOffline : public Prog
{
protected:
    static BinaryProgram _bin_program;    ///< Binary programmer instance
    static HexProgram _hex_program;       ///< HEX programmer instance

private:
    FileProgrammer _file_program;         ///< File programmer

public:
    /**
     * @brief Constructor
     */
    ProgOffline();
    
    /**
     * @brief Handle programming start
     * @param obj Programmer data object
     */
    virtual void program_start_handle(ProgData &obj) override;
    
    /**
     * @brief Get state name
     * @return "prog_offline"
     */
    virtual const char *name() override;
};