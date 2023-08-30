#ifndef __SWD_OPT_H__
#define __SWD_OPT_H__

#include <stdint.h>

#include "error.h"

dap_err_t target_opt_init(void);
dap_err_t target_opt_uninit(void);
dap_err_t target_opt_program_page(uint32_t addr, const uint8_t *buf, uint32_t size);
dap_err_t target_opt_erase_sector(uint32_t addr);
dap_err_t target_opt_erase_chip(void);


#endif // __SWD_OPT_H__
