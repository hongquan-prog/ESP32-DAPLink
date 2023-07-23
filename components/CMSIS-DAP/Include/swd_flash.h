#ifndef __SWD_FLASH_H__
#define __SWD_FLASH_H__

#include <stdint.h>

#include "error.h"

error_tt target_flash_init(uint32_t flash_start);
error_tt target_flash_uninit(void);
error_tt target_flash_program_page(uint32_t addr, const uint8_t *buf, uint32_t size);
error_tt target_flash_erase_sector(uint32_t addr);
error_tt target_flash_erase_chip(void);


#endif // __SWD_FLASH_H__
