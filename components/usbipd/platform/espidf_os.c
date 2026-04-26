/*
 * Copyright (c) 2026-2026, hongquan.li
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-3-27     hongquan.li   add ESP-IDF OSAL implementation
 */

/*
 * OSAL ESP-IDF Implementation
 *
 * OS Abstraction Layer implementation for ESP-IDF (FreeRTOS)
 */

#include <stdlib.h>
#include <string.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "hal/usbip_osal.h"

/*****************************************************************************
 * ESP-IDF Memory Operations (Low-level implementation, not using OSAL wrappers)
 *****************************************************************************/

static void* espidf_malloc(size_t size)
{
    return malloc(size);
}

static void espidf_free(void* ptr)
{
    free(ptr);
}

/*****************************************************************************
 * ESP-IDF Mutex Implementation
 *****************************************************************************/

static int espidf_mutex_init(void** handle)
{
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();

    if (mutex == NULL)
    {
        return OSAL_ERROR;
    }

    *handle = (void*)mutex;

    return OSAL_OK;
}

static int espidf_mutex_lock(void* handle)
{
    SemaphoreHandle_t mutex = (SemaphoreHandle_t)handle;

    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
    {
        return OSAL_OK;
    }

    return OSAL_ERROR;
}

static int espidf_mutex_unlock(void* handle)
{
    SemaphoreHandle_t mutex = (SemaphoreHandle_t)handle;

    if (xSemaphoreGive(mutex) == pdTRUE)
    {
        return OSAL_OK;
    }

    return OSAL_ERROR;
}

static void espidf_mutex_destroy(void* handle)
{
    SemaphoreHandle_t mutex = (SemaphoreHandle_t)handle;

    if (mutex != NULL)
    {
        vSemaphoreDelete(mutex);
    }
}

/*****************************************************************************
 * ESP-IDF Condition Variable Implementation
 * Using FreeRTOS Event Groups for condition variable simulation
 *****************************************************************************/

struct espidf_cond
{
    EventGroupHandle_t event_group;
    volatile int signaled;
    volatile int nwaiters;
    SemaphoreHandle_t lock;
};

static int espidf_cond_init(void** handle)
{
    struct espidf_cond* cond = malloc(sizeof(struct espidf_cond));

    if (cond == NULL)
    {
        return OSAL_ERROR;
    }

    cond->event_group = xEventGroupCreate();

    if (cond->event_group == NULL)
    {
        free(cond);

        return OSAL_ERROR;
    }

    cond->lock = xSemaphoreCreateMutex();

    if (cond->lock == NULL)
    {
        vEventGroupDelete(cond->event_group);
        free(cond);
        return OSAL_ERROR;
    }

    cond->signaled = 0;
    cond->nwaiters = 0;
    *handle = (void*)cond;

    return OSAL_OK;
}

static int espidf_cond_wait(void* cond_handle, void* mutex_handle)
{
    struct espidf_cond* cond = (struct espidf_cond*)cond_handle;
    SemaphoreHandle_t mutex = (SemaphoreHandle_t)mutex_handle;

    while (1)
    {
        xSemaphoreTake(cond->lock, portMAX_DELAY);
        cond->nwaiters++;
        if (cond->signaled > 0)
        {
            cond->signaled--;
            cond->nwaiters--;
            xSemaphoreGive(cond->lock);
            return OSAL_OK;
        }
        xSemaphoreGive(cond->lock);

        /* Release mutex and wait for the event */
        xSemaphoreGive(mutex);

        /* Wait for the signal. xClearOnExit (pdTRUE) clears the bit upon return,
         * so there is no need to manually ClearBits before waiting.
         * The previous ClearBits call created a lost-wakeup window. */
        xEventGroupWaitBits(cond->event_group, BIT0, pdTRUE, pdFALSE, portMAX_DELAY);

        /* Re-acquire mutex */
        xSemaphoreTake(mutex, portMAX_DELAY);

        xSemaphoreTake(cond->lock, portMAX_DELAY);
        cond->nwaiters--;
        xSemaphoreGive(cond->lock);
    }
}

static int espidf_cond_timedwait(void* cond_handle, void* mutex_handle, uint32_t timeout_ms)
{
    struct espidf_cond* cond = (struct espidf_cond*)cond_handle;
    SemaphoreHandle_t mutex = (SemaphoreHandle_t)mutex_handle;
    TickType_t ticks = pdMS_TO_TICKS(timeout_ms);
    EventBits_t bits;

    while (1)
    {
        xSemaphoreTake(cond->lock, portMAX_DELAY);
        cond->nwaiters++;
        if (cond->signaled > 0)
        {
            cond->signaled--;
            cond->nwaiters--;
            xSemaphoreGive(cond->lock);
            return OSAL_OK;
        }
        xSemaphoreGive(cond->lock);

        /* Release mutex and wait for the event */
        xSemaphoreGive(mutex);

        /* Wait for the signal with timeout. xClearOnExit (pdTRUE) clears the bit
         * upon return, removing the lost-wakeup window caused by ClearBits. */
        bits = xEventGroupWaitBits(cond->event_group, BIT0, pdTRUE, pdFALSE, ticks);

        /* Re-acquire mutex */
        xSemaphoreTake(mutex, portMAX_DELAY);

        xSemaphoreTake(cond->lock, portMAX_DELAY);
        cond->nwaiters--;
        xSemaphoreGive(cond->lock);

        if ((bits & BIT0) == 0)
        {
            return OSAL_TIMEOUT;
        }
        /* Loop back to try consume a signal if bit was set */
    }
}

static int espidf_cond_signal(void* cond_handle)
{
    struct espidf_cond* cond = (struct espidf_cond*)cond_handle;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xSemaphoreTake(cond->lock, portMAX_DELAY);
    cond->signaled++;
    xSemaphoreGive(cond->lock);

    if (xPortInIsrContext())
    {
        xEventGroupSetBitsFromISR(cond->event_group, BIT0, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    else
    {
        xEventGroupSetBits(cond->event_group, BIT0);
    }

    return OSAL_OK;
}

static int espidf_cond_broadcast(void* cond_handle)
{
    struct espidf_cond* cond = (struct espidf_cond*)cond_handle;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xSemaphoreTake(cond->lock, portMAX_DELAY);
    cond->signaled = cond->nwaiters;
    xSemaphoreGive(cond->lock);

    if (xPortInIsrContext())
    {
        xEventGroupSetBitsFromISR(cond->event_group, BIT0, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    else
    {
        xEventGroupSetBits(cond->event_group, BIT0);
    }

    return OSAL_OK;
}

static void espidf_cond_destroy(void* handle)
{
    struct espidf_cond* cond = (struct espidf_cond*)handle;

    if (cond != NULL)
    {
        if (cond->event_group != NULL)
        {
            vEventGroupDelete(cond->event_group);
        }

        if (cond->lock != NULL)
        {
            vSemaphoreDelete(cond->lock);
        }

        free(cond);
    }
}

/*****************************************************************************
 * ESP-IDF Semaphore Implementation
 *****************************************************************************/

static int espidf_sem_init(void** handle)
{
    /* Use counting semaphore to match POSIX semantics (accumulates posts) */
    SemaphoreHandle_t sem = xSemaphoreCreateCounting(portMAX_DELAY, 0);
    if (sem == NULL)
    {
        return OSAL_ERROR;
    }
    *handle = (void*)sem;
    return OSAL_OK;
}

static int espidf_sem_wait(void* handle)
{
    if (xSemaphoreTake((SemaphoreHandle_t)handle, portMAX_DELAY) == pdTRUE)
    {
        return OSAL_OK;
    }
    return OSAL_ERROR;
}

static int espidf_sem_trywait(void* handle)
{
    if (xSemaphoreTake((SemaphoreHandle_t)handle, 0) == pdTRUE)
    {
        return OSAL_OK;
    }
    return OSAL_TIMEOUT;
}

static int espidf_sem_post(void* handle)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xPortInIsrContext())
    {
        xSemaphoreGiveFromISR((SemaphoreHandle_t)handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    else
    {
        xSemaphoreGive((SemaphoreHandle_t)handle);
    }
    return OSAL_OK;
}

static void espidf_sem_destroy(void* handle)
{
    if (handle != NULL)
    {
        vSemaphoreDelete((SemaphoreHandle_t)handle);
    }
}

/*****************************************************************************
 * ESP-IDF Thread Implementation
 *****************************************************************************/

struct thread_callback 
{
    void* (*func)(void*);
    void* arg;
};

static void freertos_thread_entry(void* arg)
{
    struct thread_callback* info = (struct thread_callback*)arg;
    void* (*thread_func)(void*) = info->func;
    void* thread_arg = info->arg;

    free(info);
    thread_func(thread_arg);
    vTaskDelete(NULL);
}

static int espidf_thread_create(void** handle, const char *name,void* (*func)(void*), void* arg, size_t stack_size,
                                int priority)
{
    TaskHandle_t task_handle;
    BaseType_t ret;
    int task_priority;
    struct thread_callback* tcb = malloc(sizeof(struct thread_callback));

    if (tcb == NULL)
    {
        return OSAL_ERROR;
    }

    tcb->func = func;
    tcb->arg = arg;

    /* Use default stack size if not specified (ESP-IDF default is typically 2048) */
    if (stack_size == 0)
    {
        stack_size = 2048;
    }

    /* Priority: ESP-IDF priority range is 0-25, with higher numbers being higher priority
     * Map 0 (default) to configMAX_PRIORITIES / 2
     * Positive values increase priority, negative decrease
     */
    task_priority = configMAX_PRIORITIES / 2 + priority;

    if (task_priority < 0)
    {
        task_priority = 0;
    }

    if (task_priority >= configMAX_PRIORITIES)
    {
        task_priority = configMAX_PRIORITIES - 1;
    }

    /* Note: xTaskCreate internal allocates TCB and stack automatically.
     * We cast func to TaskFunction_t - the return value is ignored by FreeRTOS */
    ret = xTaskCreate((TaskFunction_t)freertos_thread_entry, name, stack_size / sizeof(StackType_t), tcb,
                      task_priority, &task_handle);

    if (ret != pdPASS)
    {
        free(tcb);
        return OSAL_ERROR;
    }

    *handle = (void*)task_handle;

    return OSAL_OK;
}

static int espidf_thread_join(void* handle)
{
    TaskHandle_t task = (TaskHandle_t)handle;

    /* FreeRTOS doesn't have a direct join equivalent.
     * We need to poll for task existence or use a notification mechanism.
     * For simplicity, we use eTaskGetState to check if task still exists */
    while (eTaskGetState(task) != eDeleted)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    return OSAL_OK;
}

static int espidf_thread_is_self(void* handle)
{
    TaskHandle_t task = (TaskHandle_t)handle;
    return (task == xTaskGetCurrentTaskHandle()) ? 1 : 0;
}

static int espidf_thread_detach(void* handle)
{
    /* In FreeRTOS, tasks are automatically cleaned up when they exit
     * if they were created with xTaskCreate. Nothing special to do here. */
    (void)handle;

    return OSAL_OK;
}

/*****************************************************************************
 * ESP-IDF Time Implementation
 *****************************************************************************/

static uint32_t espidf_get_time_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void espidf_sleep_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/*****************************************************************************
 * ESP-IDF Operations Interface
 *****************************************************************************/

static osal_ops_t espidf_ops = {
    /* Mutex */
    .mutex_init = espidf_mutex_init,
    .mutex_lock = espidf_mutex_lock,
    .mutex_unlock = espidf_mutex_unlock,
    .mutex_destroy = espidf_mutex_destroy,

    /* Condition variable */
    .cond_init = espidf_cond_init,
    .cond_wait = espidf_cond_wait,
    .cond_timedwait = espidf_cond_timedwait,
    .cond_signal = espidf_cond_signal,
    .cond_broadcast = espidf_cond_broadcast,
    .cond_destroy = espidf_cond_destroy,

    /* Semaphore */
    .sem_init = espidf_sem_init,
    .sem_wait = espidf_sem_wait,
    .sem_trywait = espidf_sem_trywait,
    .sem_post = espidf_sem_post,
    .sem_destroy = espidf_sem_destroy,

    /* Thread */
    .thread_create = espidf_thread_create,
    .thread_join = espidf_thread_join,
    .thread_is_self = espidf_thread_is_self,
    .thread_detach = espidf_thread_detach,

    /* Time */
    .get_time_ms = espidf_get_time_ms,
    .sleep_ms = espidf_sleep_ms,

    /* Memory */
    .malloc = espidf_malloc,
    .free = espidf_free,

    /* Platform name */
    .name = "espidf",
};

/**
 * Auto-register ESP-IDF implementation
 */
__attribute__((section(".usbip.init"), used)) void default_os_register(void)
{
    osal_register("espidf", &espidf_ops);
}
