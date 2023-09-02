#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "swd_iface.h"
// #include "flash_blob.h"

class FlashIface
{
public:
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

    typedef enum
    {
        FLASH_FUNC_NOP,
        FLASH_FUNC_ERASE,
        FLASH_FUNC_PROGRAM,
        FLASH_FUNC_VERIFY
    } func_t;

    typedef enum
    {
        FLASH_STATE_CLOSED,
        FLASH_STATE_OPEN,
        FLASH_STATE_ERROR
    } state_t;

    typedef struct
    {
        uint32_t start;
        uint32_t size;
    } sector_info_t;

    typedef struct
    {
        uint32_t init;
        uint32_t uninit;
        uint32_t erase_chip;
        uint32_t erase_sector;
        uint32_t program_page;
        uint32_t verify;
        SWDInface::syscall_t sys_call_s;
        uint32_t program_buffer;
        uint32_t algo_start;
        uint32_t algo_size;
        uint32_t *algo_blob;
        uint32_t program_buffer_size;
    } program_target_t;

    typedef enum
    {
        REIGION_DEFAULT = (1 << 0), /*!< Out of bounds regions will use the same flash algo if this is set */
        REIGION_SECURE = (1 << 1),  /*!< The region can only be accessed from the secure world. Only applies for TrustZone-enabled targets. */
    } reigion_flag_t;

    typedef struct
    {
        uint32_t start;                     /*!< Region start address. */
        uint32_t end;                       /*!< Region end address. */
        uint32_t flags;                     /*!< Flags for this region from the #reigion_flag_t enumeration. */
        const program_target_t *flash_algo; /*!< A pointer to the flash algorithm structure */
    } region_info_t;

    typedef struct
    {
        std::vector<sector_info_t> sector_info;   /*!< Sector start and length list */
        std::vector<region_info_t> flash_regions; /*!< Flash regions */
        std::vector<region_info_t> ram_regions;   /*!< RAM regions  */
        uint8_t erase_reset;                      /*!< Reset after performing an erase */
        std::string device_name;                  /*!< Device name and description */
    } target_cfg_t;

    virtual ~FlashIface() = default;
    virtual void swd_init(SWDInface &swd) = 0;
    virtual err_t flash_init(const target_cfg_t &cfg) = 0;
    virtual err_t flash_uninit(void) = 0;
    virtual err_t flash_program_page(uint32_t addr, const uint8_t *buf, uint32_t size) = 0;
    virtual err_t flash_erase_sector(uint32_t sector) = 0;
    virtual err_t flash_erase_chip(void) = 0;
    virtual uint32_t flash_program_page_min_size(uint32_t addr) = 0;
    virtual uint32_t flash_erase_sector_size(uint32_t addr) = 0;
    virtual uint8_t flash_busy(void) = 0;
    virtual err_t flash_algo_set(uint32_t addr) = 0;
};
