#pragma once

#include <stdint.h>
#include "flash_blob.h"

//! @brief Current target configuration version.
//!
//! - Version 1: Initial version.
enum _target_config_version {
    kTargetConfigVersion = 1, //!< The current board info version.
};

//! This can vary from target to target and should be in the structure or flash blob
#define TARGET_AUTO_INCREMENT_PAGE_SIZE    (1024)

//! Additional flash and ram regions
#define MAX_REGIONS (4)

//! @brief Option flags for memory regions.
enum _region_flags {
    kRegionIsDefault = (1 << 0), /*!< Out of bounds regions will use the same flash algo if this is set */
    kRegionIsSecure  = (1 << 1), /*!< The region can only be accessed from the secure world. Only applies for TrustZone-enabled targets. */
};

/*!
 * @brief Details of a target flash or RAM memory region.
 */
typedef struct __attribute__((__packed__)) region_info {
    uint32_t start;                 /*!< Region start address. */
    uint32_t end;                   /*!< Region end address. */
    uint32_t flags;                 /*!< Flags for this region from the #_region_flags enumeration. */
    uint32_t alias_index;           /*!< Use with flags; will point to a different index if there is an alias region */
    program_target_t *flash_algo;   /*!< A pointer to the flash algorithm structure */
} region_info_t;

/*!
 * @brief Information required to program target flash.
 */
typedef struct __attribute__((__packed__)) target_cfg {
    uint32_t version;                           /*!< Target configuration version */
    const sector_info_t* sectors_info;          /*!< Sector start and length list */
    uint32_t sector_info_length;                /*!< Number of entries in the sectors_info array */
    region_info_t flash_regions[MAX_REGIONS];   /*!< Flash regions */
    region_info_t ram_regions[MAX_REGIONS];     /*!< RAM regions  */
} target_cfg_t;
