#pragma once

#include <cstdint>
#include "flash_blob.h"

#define MAX_REGIONS (4) /*!< Additional flash and ram regions. */

typedef enum
{
    FLASH_FUNC_NOP,
    FLASH_FUNC_ERASE,
    FLASH_FUNC_PROGRAM,
    FLASH_FUNC_VERIFY
} flash_func_t;

typedef enum
{
    FLASH_STATE_CLOSED,
    FLASH_STATE_OPEN,
    FLASH_STATE_ERROR
} flash_state_t;

enum _target_config_version
{
    kTargetConfigVersion = 1, //!< The current board info version.
};

enum _region_flags
{
    kRegionIsDefault = (1 << 0), /*!< Out of bounds regions will use the same flash algo if this is set */
    kRegionIsSecure = (1 << 1),  /*!< The region can only be accessed from the secure world. Only applies for TrustZone-enabled targets. */
};

typedef struct
{
    uint32_t start;                     /*!< Region start address. */
    uint32_t end;                       /*!< Region end address. */
    uint32_t flags;                     /*!< Flags for this region from the #_region_flags enumeration. */
    const program_target_t *flash_algo; /*!< A pointer to the flash algorithm structure */
} region_info_t;

typedef struct
{
    uint32_t version;                         /*!< Target configuration version */
    const sector_info_t *sectors_info;        /*!< Sector start and length list */
    uint32_t sector_info_length;              /*!< Number of entries in the sectors_info array */
    region_info_t flash_regions[MAX_REGIONS]; /*!< Flash regions */
    region_info_t ram_regions[MAX_REGIONS];   /*!< RAM regions  */
    uint8_t erase_reset;                      /*!< Reset after performing an erase */
    char device_name[60];                     /*!< Part number of the target device. Must match the Dname attribute value  of the device's CMSIS DFP. Maximum 60 characters including terminal NULL. */
} target_cfg_t;

class Flash
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

    virtual ~Flash() = default;
    virtual err_t flash_init(const target_cfg_t *cfg) = 0;
    virtual err_t flash_uninit(void) = 0;
    virtual err_t flash_program_page(uint32_t addr, const uint8_t *buf, uint32_t size) = 0;
    virtual err_t flash_erase_sector(uint32_t sector) = 0;
    virtual err_t flash_erase_chip(void) = 0;
    virtual uint32_t flash_program_page_min_size(uint32_t addr) = 0;
    virtual uint32_t flash_erase_sector_size(uint32_t addr) = 0;
    virtual uint8_t flash_busy(void) = 0;
    virtual err_t flash_algo_set(uint32_t addr) = 0;
};
