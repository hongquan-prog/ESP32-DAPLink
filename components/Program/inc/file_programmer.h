/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   Initial version
 * 2026-3-16     Refactor     Improved interface and callback mechanism
 */
#pragma once

#include "program_iface.h"
#include <functional>
#include <string>

/**
 * @brief File-based flash programmer
 * 
 * This class handles programming flash from files (BIN or HEX format).
 * It automatically selects the appropriate programmer based on file extension.
 */
class FileProgrammer
{
public:
    /**
     * @brief Callback function type for progress updates
     * @param progress Progress percentage (0-100)
     */
    using progress_changed_cb_t = std::function<void(int progress)>;

private:
    ProgramIface &_binary_program;    ///< Binary file programmer
    ProgramIface &_hex_program;       ///< HEX file programmer
    int _program_progress;            ///< Current progress percentage
    progress_changed_cb_t _progress_changed_cb;  ///< Progress callback

    static constexpr int _buf_size = 256;  ///< Buffer size for file reading
    uint8_t _buffer[_buf_size];            ///< File read buffer

    /**
     * @brief Select programmer based on file extension
     * @param path File path
     * @return Pointer to selected programmer
     */
    ProgramIface *select_program_iface(const std::string &path);
    
    /**
     * @brief Update progress and notify callback
     * @param progress Progress percentage
     */
    void set_program_progress(int progress);

public:
    /**
     * @brief Constructor
     * @param binary_program Binary file programmer instance
     * @param hex_program HEX file programmer instance
     */
    FileProgrammer(ProgramIface &binary_program, ProgramIface &hex_program);
    
    /**
     * @brief Program a file to flash
     * @param path Path to the file to program
     * @param cfg Target flash configuration
     * @param program_addr Optional start address for programming
     * @return true if programming successful
     */
    bool program(const std::string &path, FlashIface::target_cfg_t &cfg, uint32_t program_addr = 0);
    
    /**
     * @brief Get current programming progress
     * @return Progress percentage (0-100)
     */
    int get_program_progress(void);
    
    /**
     * @brief Register progress callback function
     * @param func Callback function
     */
    void register_progress_changed_callback(const progress_changed_cb_t &func);
    
    /**
     * @brief Check if a file exists
     * @param path File path to check
     * @return true if file exists
     */
    static bool is_exist(const char *path);
    
    /**
     * @brief Compare file extension
     * @param filename Filename to check
     * @param extension Extension to compare (e.g., ".hex")
     * @return true if extension matches
     */
    static bool compare_extension(const char *filename, const char *extension);
};
