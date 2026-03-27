/*
 * Copyright (c) 2026-2026, hongquan.li
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-3-24      hongquan.li   add license declaration
 */

#ifndef USBIP_OSAL_H
#define USBIP_OSAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * OS Abstraction Layer (OSAL)
 *
 * Purpose: Provide portable OS primitives interface, supporting POSIX, FreeRTOS,
 *          bare-metal and other platforms
 * Memory: Prefer static allocation, avoid dynamic memory
 *****************************************************************************/

#define OSAL_OK 0
#define OSAL_ERROR -1
#define OSAL_TIMEOUT -2

/*****************************************************************************
 * OSAL Operations Interface Structure
 *****************************************************************************/

typedef struct osal_ops osal_ops_t;

struct osal_ops
{
    /* Mutex */
    int (*mutex_init)(void* handle);
    int (*mutex_lock)(void* handle);
    int (*mutex_unlock)(void* handle);
    void (*mutex_destroy)(void* handle);

    /* Condition variable */
    int (*cond_init)(void* handle);
    int (*cond_wait)(void* cond, void* mutex);
    int (*cond_timedwait)(void* cond, void* mutex, uint32_t timeout_ms);
    int (*cond_signal)(void* cond);
    int (*cond_broadcast)(void* cond);
    void (*cond_destroy)(void* handle);

    /* Thread */
    int (*thread_create)(void* handle, void* (*func)(void*), void* arg, size_t stack_size,
                         int priority);
    int (*thread_join)(void* handle);
    int (*thread_detach)(void* handle);
    int (*thread_delete)(void* handle);

    /* Time */
    uint32_t (*get_time_ms)(void);
    void (*sleep_ms)(uint32_t ms);

    /* Memory */
    void* (*malloc)(size_t size);
    void (*free)(void* ptr);

    /* Platform name */
    const char* name;
};

/*****************************************************************************
 * OSAL Registration Mechanism
 *****************************************************************************/

/**
 * osal_register - Register platform implementation
 * @platform: Platform name
 * @ops: Operations interface pointer
 * Return: OSAL_OK on success, error code on failure
 */
int osal_register(const char* platform, const osal_ops_t* ops);

/**
 * osal_get_ops - Get current platform's OSAL operations interface
 * Return: Operations interface pointer
 */
const osal_ops_t* osal_get_ops(void);

/**
 * OSAL_REGISTER - Auto-registration macro
 * @platform: Platform name
 * @ops: osal_ops_t variable name
 */
#define OSAL_REGISTER(platform, ops)                                                               \
    __attribute__((constructor, used)) void osal_register_##platform(void)                        \
    {                                                                                              \
        osal_register(#platform, &ops);                                                            \
    }

/*****************************************************************************
 * Unified Interface Wrapper Functions
 *****************************************************************************/

struct osal_mutex
{
    void* handle; /* Platform-specific handle */
};

/**
 * osal_mutex_init - Initialize mutex
 * @mutex: Mutex pointer
 * Return: OSAL_OK on success, error code on failure
 */
int osal_mutex_init(struct osal_mutex* mutex);

/**
 * osal_mutex_lock - Lock mutex
 * @mutex: Mutex pointer
 * Return: OSAL_OK on success, error code on failure
 */
int osal_mutex_lock(struct osal_mutex* mutex);

/**
 * osal_mutex_unlock - Unlock mutex
 * @mutex: Mutex pointer
 * Return: OSAL_OK on success, error code on failure
 */
int osal_mutex_unlock(struct osal_mutex* mutex);

/**
 * osal_mutex_destroy - Destroy mutex
 * @mutex: Mutex pointer
 */
void osal_mutex_destroy(struct osal_mutex* mutex);

struct osal_cond
{
    void* handle; /* Platform-specific handle */
};

/**
 * osal_cond_init - Initialize condition variable
 * @cond: Condition variable pointer
 * Return: OSAL_OK on success, error code on failure
 */
int osal_cond_init(struct osal_cond* cond);

/**
 * osal_cond_wait - Wait on condition variable
 * @cond: Condition variable pointer
 * @mutex: Associated mutex (must be locked before calling)
 * Return: OSAL_OK on success, error code on failure
 */
int osal_cond_wait(struct osal_cond* cond, struct osal_mutex* mutex);

/**
 * osal_cond_timedwait - Wait on condition variable with timeout
 * @cond: Condition variable pointer
 * @mutex: Associated mutex
 * @timeout_ms: Timeout in milliseconds
 * Return: OSAL_OK on success, OSAL_TIMEOUT on timeout, error code on failure
 */
int osal_cond_timedwait(struct osal_cond* cond, struct osal_mutex* mutex, uint32_t timeout_ms);

/**
 * osal_cond_signal - Wake one waiting thread
 * @cond: Condition variable pointer
 * Return: OSAL_OK on success, error code on failure
 */
int osal_cond_signal(struct osal_cond* cond);

/**
 * osal_cond_broadcast - Wake all waiting threads
 * @cond: Condition variable pointer
 * Return: OSAL_OK on success, error code on failure
 */
int osal_cond_broadcast(struct osal_cond* cond);

/**
 * osal_cond_destroy - Destroy condition variable
 * @cond: Condition variable pointer
 */
void osal_cond_destroy(struct osal_cond* cond);

typedef void* (*osal_thread_func)(void* arg);

struct osal_thread
{
    void* handle; /* Platform-specific handle */
};

/**
 * osal_thread_create - Create thread
 * @thread: Thread structure pointer
 * @func: Thread entry function
 * @arg: Argument passed to thread
 * @stack_size: Stack size in bytes, 0 for default
 * @priority: Priority (0=default, positive=higher priority)
 * Return: OSAL_OK on success, error code on failure
 */
int osal_thread_create(struct osal_thread* thread, osal_thread_func func, void* arg,
                       size_t stack_size, int priority);

/**
 * osal_thread_join - Wait for thread to finish
 * @thread: Thread structure pointer
 * Return: OSAL_OK on success, error code on failure
 */
int osal_thread_join(struct osal_thread* thread);

/**
 * osal_thread_detach - Detach thread
 * @thread: Thread structure pointer
 * Return: OSAL_OK on success, error code on failure
 */
int osal_thread_detach(struct osal_thread* thread);

/**
 * osal_thread_delete - Delete thread (for FreeRTOS vTaskDelete)
 * @thread: Thread structure pointer
 * Return: OSAL_OK on success, error code on failure
 */
int osal_thread_delete(struct osal_thread* thread);

/**
 * osal_get_time_ms - Get current time in milliseconds
 * Return: Milliseconds since system startup
 */
uint32_t osal_get_time_ms(void);

/**
 * osal_sleep_ms - Sleep for specified milliseconds
 * @ms: Sleep duration in milliseconds
 */
void osal_sleep_ms(uint32_t ms);

/**
 * osal_malloc - Allocate memory
 * @size: Size to allocate in bytes
 * Return: Allocated pointer, NULL on failure
 */
void* osal_malloc(size_t size);

/**
 * osal_free - Free memory
 * @ptr: Pointer to free
 */
void osal_free(void* ptr);

struct osal_mempool
{
    uint8_t* buffer;    /* Memory buffer */
    size_t block_size;  /* Block size */
    size_t block_count; /* Number of blocks */
    size_t* free_list;  /* Free block indices */
    size_t free_count;  /* Number of free blocks */
    struct osal_mutex lock;
};

/**
 * osal_mempool_init - Initialize static memory pool
 * @pool: Memory pool pointer
 * @buffer: Pre-allocated memory buffer
 * @block_size: Size of each block
 * @block_count: Number of blocks
 * Return: OSAL_OK on success, error code on failure
 */
int osal_mempool_init(struct osal_mempool* pool, void* buffer, size_t block_size,
                      size_t block_count);

/**
 * osal_mempool_alloc - Allocate block from pool
 * @pool: Memory pool pointer
 * Return: Allocated block pointer, NULL if no blocks available
 */
void* osal_mempool_alloc(struct osal_mempool* pool);

/**
 * osal_mempool_free - Free block back to pool
 * @pool: Memory pool pointer
 * @ptr: Block pointer to free
 */
void osal_mempool_free(struct osal_mempool* pool, void* ptr);

/**
 * osal_mempool_destroy - Destroy memory pool
 * @pool: Memory pool pointer
 */
void osal_mempool_destroy(struct osal_mempool* pool);

#ifdef __cplusplus
}
#endif

#endif /* USBIP_OSAL_H */