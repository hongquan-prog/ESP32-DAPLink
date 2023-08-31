#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t breakpoint;
    uint32_t static_base;
    uint32_t stack_pointer;
} program_syscall_t;

typedef struct
{
    uint32_t init;
    uint32_t uninit;
    uint32_t erase_chip;
    uint32_t erase_sector;
    uint32_t program_page;
    uint32_t verify;
    program_syscall_t sys_call_s;
    uint32_t program_buffer;
    uint32_t algo_start;
    uint32_t algo_size;
    uint32_t *algo_blob;
    uint32_t program_buffer_size;
} program_target_t;

typedef struct
{
    uint32_t start;
    uint32_t size;
} sector_info_t;

#ifdef __cplusplus
}
#endif