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
 * OSAL Core
 *
 * OS Abstraction Layer core implementation: registry + interface call entry
 */

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "hal/usbip_log.h"
#include "hal/usbip_osal.h"

LOG_MODULE_REGISTER(osal, CONFIG_USBIP_LOG_LEVEL);

/*****************************************************************************
 * Global Variables
 *****************************************************************************/

static const osal_ops_t* s_osal_ops = NULL;
static char s_platform_name[32] = {0};

/*****************************************************************************
 * Registration and Get Interface
 *****************************************************************************/

int osal_register(const char* platform, const osal_ops_t* ops)
{
    if (!platform || !ops)
    {
        LOG_ERR("osal_register: platform or ops is NULL");
        return OSAL_ERROR;
    }

    if (s_osal_ops != NULL)
    {
        LOG_ERR("osal_register: osal already registered");
        return OSAL_ERROR;
    }

    s_osal_ops = ops;
    strncpy(s_platform_name, platform, sizeof(s_platform_name) - 1);
    s_platform_name[sizeof(s_platform_name) - 1] = '\0';
    LOG_INF("osal_register: platform %s", s_platform_name);

    return OSAL_OK;
}

const osal_ops_t* osal_get_ops(void)
{
    return s_osal_ops;
}

/*****************************************************************************
 * Mutex Wrapper Functions
 *****************************************************************************/

int osal_mutex_init(struct osal_mutex* mutex)
{
    const osal_ops_t* ops = osal_get_ops();

    if (!ops || !ops->mutex_init)
    {
        return OSAL_ERROR;
    }

    pthread_mutex_t* pm = ops->malloc(sizeof(pthread_mutex_t));

    if (!pm)
    {
        return OSAL_ERROR;
    }

    if (ops->mutex_init(pm) != OSAL_OK)
    {
        ops->free(pm);
        return OSAL_ERROR;
    }

    mutex->handle = pm;
    return OSAL_OK;
}

int osal_mutex_lock(struct osal_mutex* mutex)
{
    const osal_ops_t* ops = osal_get_ops();

    if (!ops || !ops->mutex_lock || !mutex->handle)
    {
        return OSAL_ERROR;
    }
    return ops->mutex_lock(mutex->handle);
}

int osal_mutex_unlock(struct osal_mutex* mutex)
{
    const osal_ops_t* ops = osal_get_ops();

    if (!ops || !ops->mutex_unlock || !mutex->handle)
    {
        return OSAL_ERROR;
    }
    return ops->mutex_unlock(mutex->handle);
}

void osal_mutex_destroy(struct osal_mutex* mutex)
{
    const osal_ops_t* ops = osal_get_ops();

    if (!ops || !mutex->handle)
    {
        return;
    }

    if (ops->mutex_destroy)
    {
        ops->mutex_destroy(mutex->handle);
    }
    if (ops->free)
    {
        ops->free(mutex->handle);
    }
    mutex->handle = NULL;
}

/*****************************************************************************
 * Condition Variable Wrapper Functions
 *****************************************************************************/

int osal_cond_init(struct osal_cond* cond)
{
    const osal_ops_t* ops = osal_get_ops();

    if (!ops || !ops->cond_init)
    {
        return OSAL_ERROR;
    }

    pthread_cond_t* pc = ops->malloc(sizeof(pthread_cond_t));

    if (!pc)
    {
        return OSAL_ERROR;
    }

    if (ops->cond_init(pc) != OSAL_OK)
    {
        ops->free(pc);
        return OSAL_ERROR;
    }

    cond->handle = pc;
    return OSAL_OK;
}

int osal_cond_wait(struct osal_cond* cond, struct osal_mutex* mutex)
{
    const osal_ops_t* ops = osal_get_ops();

    if (!ops || !ops->cond_wait || !cond->handle || !mutex->handle)
    {
        return OSAL_ERROR;
    }
    return ops->cond_wait(cond->handle, mutex->handle);
}

int osal_cond_timedwait(struct osal_cond* cond, struct osal_mutex* mutex, uint32_t timeout_ms)
{
    const osal_ops_t* ops = osal_get_ops();

    if (!ops || !ops->cond_timedwait || !cond->handle || !mutex->handle)
    {
        return OSAL_ERROR;
    }
    return ops->cond_timedwait(cond->handle, mutex->handle, timeout_ms);
}

int osal_cond_signal(struct osal_cond* cond)
{
    const osal_ops_t* ops = osal_get_ops();

    if (!ops || !ops->cond_signal || !cond->handle)
    {
        return OSAL_ERROR;
    }
    return ops->cond_signal(cond->handle);
}

int osal_cond_broadcast(struct osal_cond* cond)
{
    const osal_ops_t* ops = osal_get_ops();

    if (!ops || !ops->cond_broadcast || !cond->handle)
    {
        return OSAL_ERROR;
    }
    return ops->cond_broadcast(cond->handle);
}

void osal_cond_destroy(struct osal_cond* cond)
{
    const osal_ops_t* ops = osal_get_ops();

    if (!ops || !cond->handle)
    {
        return;
    }

    if (ops->cond_destroy)
    {
        ops->cond_destroy(cond->handle);
    }
    if (ops->free)
    {
        ops->free(cond->handle);
    }
    cond->handle = NULL;
}

/*****************************************************************************
 * Thread Wrapper Functions
 *****************************************************************************/

int osal_thread_create(struct osal_thread* thread, osal_thread_func func, void* arg,
                       size_t stack_size, int priority)
{
    const osal_ops_t* ops = osal_get_ops();

    if (!ops || !ops->thread_create)
    {
        return OSAL_ERROR;
    }

    pthread_t* pt = ops->malloc(sizeof(pthread_t));

    if (!pt)
    {
        return OSAL_ERROR;
    }

    if (ops->thread_create(pt, (void* (*)(void*))func, arg, stack_size, priority) != OSAL_OK)
    {
        ops->free(pt);
        return OSAL_ERROR;
    }

    thread->handle = pt;
    return OSAL_OK;
}

int osal_thread_join(struct osal_thread* thread)
{
    const osal_ops_t* ops = osal_get_ops();

    if (!ops || !ops->thread_join || !thread->handle)
    {
        return OSAL_ERROR;
    }

    int ret = ops->thread_join(thread->handle);
    if (ret == OSAL_OK && ops->free)
    {
        ops->free(thread->handle);
        thread->handle = NULL;
    }
    return ret;
}

int osal_thread_detach(struct osal_thread* thread)
{
    const osal_ops_t* ops = osal_get_ops();

    if (!ops || !ops->thread_detach || !thread->handle)
    {
        return OSAL_ERROR;
    }

    int ret = ops->thread_detach(thread->handle);
    if (ret == OSAL_OK && ops->free)
    {
        ops->free(thread->handle);
        thread->handle = NULL;
    }
    return ret;
}

int osal_thread_delete(struct osal_thread* thread)
{
    const osal_ops_t* ops = osal_get_ops();

    if (!ops || !ops->thread_delete || !thread->handle)
    {
        return OSAL_ERROR;
    }

    return ops->thread_delete(thread->handle);
}

/*****************************************************************************
 * Time Wrapper Functions
 *****************************************************************************/

uint32_t osal_get_time_ms(void)
{
    const osal_ops_t* ops = osal_get_ops();

    if (!ops || !ops->get_time_ms)
    {
        return 0;
    }
    return ops->get_time_ms();
}

void osal_sleep_ms(uint32_t ms)
{
    const osal_ops_t* ops = osal_get_ops();

    if (ops && ops->sleep_ms)
    {
        ops->sleep_ms(ms);
    }
}

/*****************************************************************************
 * Memory Wrapper Functions
 *****************************************************************************/

void* osal_malloc(size_t size)
{
    const osal_ops_t* ops = osal_get_ops();

    if (!ops || !ops->malloc)
    {
        return NULL;
    }
    return ops->malloc(size);
}

void osal_free(void* ptr)
{
    const osal_ops_t* ops = osal_get_ops();

    if (ops && ops->free && ptr)
    {
        ops->free(ptr);
    }
}