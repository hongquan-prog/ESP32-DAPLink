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
#include <vector>
#include <string>
#include <memory>
#include "swd_iface.h"

/**
 * @brief Flash memory interface for target device programming
 * 
 * This class defines the interface for flash memory operations including
 * initialization, erasing, programming, and verification.
 */
class FlashIface
{
public:
    /**
     * @brief Flash operation error codes
     */
    typedef enum
    {
        /* Shared errors */
        ERR_NONE = 0,
        ERR_FAILURE,
        ERR_INTERNAL,

        /* Target flash errors */
        ERR_RESET,
        ERR_ALGO_DL,
        ERR_ALGO_MISSING,
        ERR_ALGO_DATA_SEQ,
        ERR_INIT,
        ERR_UNINIT,
        ERR_SECURITY_BITS,
        ERR_UNLOCK,
        ERR_ERASE_SECTOR,
        ERR_ERASE_ALL,
        ERR_WRITE,
        ERR_WRITE_VERIFY,

        ERR_COUNT
    } err_t;

    /**
     * @brief Flash function codes for algorithm execution
     */
    typedef enum
    {
        FLASH_FUNC_NOP,
        FLASH_FUNC_ERASE,
        FLASH_FUNC_PROGRAM,
        FLASH_FUNC_VERIFY
    } func_t;

    /**
     * @brief Flash state machine states
     */
    typedef enum
    {
        FLASH_STATE_CLOSED,
        FLASH_STATE_OPEN,
        FLASH_STATE_ERROR
    } state_t;

    /**
     * @brief Information about a flash sector
     */
    typedef struct
    {
        uint32_t start;     ///< Sector start address
        uint32_t size;      ///< Sector size in bytes
    } sector_info_t;

    /**
     * @brief System call configuration for flash algorithm
     */
    typedef struct
    {
        uint32_t init;              ///< Init function address
        uint32_t uninit;            ///< Uninit function address
        uint32_t erase_chip;        ///< Chip erase function address
        uint32_t erase_sector;      ///< Sector erase function address
        uint32_t program_page;      ///< Page program function address
        uint32_t verify;            ///< Verify function address
        SWDIface::syscall_t sys_call_s; ///< System call parameters
        uint32_t program_buffer;    ///< Program buffer address
        uint32_t algo_start;        ///< Algorithm start address
        uint32_t algo_size;         ///< Algorithm size in bytes
        uint32_t *algo_blob;        ///< Pointer to algorithm blob
        uint32_t program_buffer_size; ///< Program buffer size
    } program_target_t;

    /**
     * @brief Region flags for memory regions
     */
    typedef enum
    {
        REGION_DEFAULT = (1 << 0),  ///< Out of bounds regions will use the same flash algo if this is set
        REGION_SECURE = (1 << 1),   ///< The region can only be accessed from the secure world
    } region_flag_t;

    /**
     * @brief Information about a memory region
     */
    typedef struct
    {
        uint32_t start;                     ///< Region start address
        uint32_t end;                       ///< Region end address
        uint32_t flags;                     ///< Region flags from #region_flag_t
        const program_target_t *flash_algo; ///< Pointer to flash algorithm structure
    } region_info_t;

    /**
     * @brief Target device configuration
     */
    typedef struct
    {
        std::vector<sector_info_t> sector_info;   ///< Sector start and length list
        std::vector<region_info_t> flash_regions; ///< Flash regions
        std::vector<region_info_t> ram_regions;   ///< RAM regions
        uint8_t erase_reset;                      ///< Reset after performing an erase
        std::string device_name;                  ///< Device name and description
    } target_cfg_t;

    virtual ~FlashIface() = default;
    
    /**
     * @brief Initialize SWD interface
     * @param swd SWD interface instance
     */
    virtual void swd_init(SWDIface &swd) = 0;
    
    /**
     * @brief Initialize flash interface with target configuration
     * @param cfg Target configuration
     * @return ERR_NONE on success
     */
    virtual err_t flash_init(const target_cfg_t &cfg) = 0;
    
    /**
     * @brief Uninitialize flash interface
     * @return ERR_NONE on success
     */
    virtual err_t flash_uninit(void) = 0;
    
    /**
     * @brief Program a page in flash memory
     * @param addr Destination address
     * @param buf Source buffer
     * @param size Number of bytes to program
     * @return ERR_NONE on success
     */
    virtual err_t flash_program_page(uint32_t addr, const uint8_t *buf, uint32_t size) = 0;
    
    /**
     * @brief Erase a flash sector
     * @param sector Sector address
     * @return ERR_NONE on success
     */
    virtual err_t flash_erase_sector(uint32_t sector) = 0;
    
    /**
     * @brief Erase entire flash chip
     * @return ERR_NONE on success
     */
    virtual err_t flash_erase_chip(void) = 0;
    
    /**
     * @brief Get minimum program page size at given address
     * @param addr Address to check
     * @return Minimum page size in bytes
     */
    virtual uint32_t flash_program_page_min_size(uint32_t addr) = 0;
    
    /**
     * @brief Get flash sector size at given address
     * @param addr Address to check
     * @return Sector size in bytes
     */
    virtual uint32_t flash_erase_sector_size(uint32_t addr) = 0;
    
    /**
     * @brief Check if flash operation is in progress
     * @return true if busy
     */
    virtual uint8_t flash_busy(void) = 0;
    
    /**
     * @brief Set flash algorithm at given address
     * @param addr Algorithm address
     * @return ERR_NONE on success
     */
    virtual err_t flash_algo_set(uint32_t addr) = 0;
};
