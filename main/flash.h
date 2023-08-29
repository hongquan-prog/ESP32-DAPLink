#pragma once

#include <cstdint>
#include "error.h"
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

enum _region_flags
{
    kRegionIsDefault = (1 << 0), /*!< Out of bounds regions will use the same flash algo if this is set */
    kRegionIsSecure = (1 << 1),  /*!< The region can only be accessed from the secure world. Only applies for TrustZone-enabled targets. */
};

typedef struct __attribute__((__packed__))
{
    uint32_t start;               /*!< Region start address. */
    uint32_t end;                 /*!< Region end address. */
    uint32_t flags;               /*!< Flags for this region from the #_region_flags enumeration. */
    uint32_t alias_index;         /*!< Use with flags; will point to a different index if there is an alias region */
    program_target_t *flash_algo; /*!< A pointer to the flash algorithm structure */
} region_info_t;

typedef struct __attribute__((__packed__))
{
    uint32_t version;                         /*!< Target configuration version */
    const sector_info_t *sectors_info;        /*!< Sector start and length list */
    uint32_t sector_info_length;              /*!< Number of entries in the sectors_info array */
    region_info_t flash_regions[MAX_REGIONS]; /*!< Flash regions */
    region_info_t ram_regions[MAX_REGIONS];   /*!< RAM regions  */
    const char *rt_board_id;                  /*!< If assigned, this is a flexible board ID */
    uint16_t rt_family_id;                    /*!< If assigned, this is a flexible family ID */
    uint8_t erase_reset;                      /*!< Reset after performing an erase */
    uint8_t pad;

    /*!< CMSIS-DAP v2.1 target strings */
    char *target_vendor;      /*!< Must match the Dvendor attribute value of the CMSIS DFP, excluding the
                                   colon and vendor code suffix when present. Maximum 60 characters including
                                   terminal NULL. */
    char *target_part_number; /*!< Part number of the target device. Must match the Dname attribute value
                                   of the device's CMSIS DFP. Maximum 60 characters including terminal NULL. */
} target_cfg_t;

class Flash
{
public:
    virtual ~Flash() = default;
    virtual error_t flash_init(target_cfg_t *cfg) = 0;
    virtual error_t flash_uninit(void) = 0;
    virtual error_t flash_program_page(uint32_t addr, const uint8_t *buf, uint32_t size) = 0;
    virtual error_t flash_erase_sector(uint32_t sector) = 0;
    virtual error_t flash_erase_chip(void) = 0;
    virtual uint32_t flash_program_page_min_size(uint32_t addr) = 0;
    virtual uint32_t flash_erase_sector_size(uint32_t addr) = 0;
    virtual uint8_t flash_busy(void) = 0;
    virtual error_t flash_algo_set(uint32_t addr) = 0;
};
