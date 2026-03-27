/*
 * Copyright (c) 2026-2026, hongquan.li
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-3-24      hongquan.li   add license declaration
 */

/*
 * OSAL Memory Pool Implementation
 *
 * Static memory pool implementation, using unified OSAL memory interface
 */

#include <stdint.h>
#include "hal/usbip_osal.h"

/*****************************************************************************
 * Static Memory Pool Implementation
 *****************************************************************************/

int osal_mempool_init(struct osal_mempool* pool, void* buffer, size_t block_size,
                      size_t block_count)
{
    if (!pool || !buffer || block_size == 0 || block_count == 0)
    {
        return OSAL_ERROR;
    }

    pool->buffer = (uint8_t*)buffer;
    pool->block_size = block_size;
    pool->block_count = block_count;

    /* Allocate free list (using OSAL memory interface) */
    pool->free_list = osal_malloc(block_count * sizeof(size_t));
    if (!pool->free_list)
    {
        return OSAL_ERROR;
    }

    /* Initialize free list */
    for (size_t i = 0; i < block_count; i++)
    {
        pool->free_list[i] = i;
    }
    pool->free_count = block_count;

    /* Initialize mutex */
    if (osal_mutex_init(&pool->lock) != OSAL_OK)
    {
        osal_free(pool->free_list);
        pool->free_list = NULL;
        return OSAL_ERROR;
    }

    return OSAL_OK;
}

void* osal_mempool_alloc(struct osal_mempool* pool)
{
    if (!pool || !pool->free_list)
    {
        return NULL;
    }

    osal_mutex_lock(&pool->lock);

    if (pool->free_count == 0)
    {
        osal_mutex_unlock(&pool->lock);
        return NULL;
    }

    size_t idx = pool->free_list[--pool->free_count];
    void* ptr = pool->buffer + idx * pool->block_size;

    osal_mutex_unlock(&pool->lock);
    return ptr;
}

void osal_mempool_free(struct osal_mempool* pool, void* ptr)
{
    if (!pool || !pool->free_list || !ptr)
    {
        return;
    }

    uintptr_t offset = (uint8_t*)ptr - pool->buffer;
    size_t idx = offset / pool->block_size;

    if (idx >= pool->block_count)
    {
        /* Not in this pool */
        return;
    }

    osal_mutex_lock(&pool->lock);
    pool->free_list[pool->free_count++] = idx;
    osal_mutex_unlock(&pool->lock);
}

void osal_mempool_destroy(struct osal_mempool* pool)
{
    if (!pool)
    {
        return;
    }

    osal_mutex_destroy(&pool->lock);
    osal_free(pool->free_list);
    pool->free_list = NULL;
    pool->buffer = NULL;
    pool->free_count = 0;
}