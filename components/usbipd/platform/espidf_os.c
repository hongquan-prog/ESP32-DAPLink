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

    cond->signaled = 0;
    *handle = (void*)cond;

    return OSAL_OK;
}

static int espidf_cond_wait(void* cond_handle, void* mutex_handle)
{
    struct espidf_cond* cond = (struct espidf_cond*)cond_handle;
    SemaphoreHandle_t mutex = (SemaphoreHandle_t)mutex_handle;

    /* Clear the event bit before waiting */
    xEventGroupClearBits(cond->event_group, BIT0);

    /* Release mutex and wait for the event */
    xSemaphoreGive(mutex);

    /* Wait for the signal */
    xEventGroupWaitBits(cond->event_group, BIT0, pdTRUE, pdFALSE, portMAX_DELAY);

    /* Re-acquire mutex */
    xSemaphoreTake(mutex, portMAX_DELAY);

    return OSAL_OK;
}

static int espidf_cond_timedwait(void* cond_handle, void* mutex_handle, uint32_t timeout_ms)
{
    struct espidf_cond* cond = (struct espidf_cond*)cond_handle;
    SemaphoreHandle_t mutex = (SemaphoreHandle_t)mutex_handle;
    TickType_t ticks = pdMS_TO_TICKS(timeout_ms);
    EventBits_t bits;

    /* Clear the event bit before waiting */
    xEventGroupClearBits(cond->event_group, BIT0);

    /* Release mutex and wait for the event */
    xSemaphoreGive(mutex);

    /* Wait for the signal with timeout */
    bits = xEventGroupWaitBits(cond->event_group, BIT0, pdTRUE, pdFALSE, ticks);

    /* Re-acquire mutex */
    xSemaphoreTake(mutex, portMAX_DELAY);

    if ((bits & BIT0) == 0)
    {
        return OSAL_TIMEOUT;
    }

    return OSAL_OK;
}

static int espidf_cond_signal(void* cond_handle)
{
    struct espidf_cond* cond = (struct espidf_cond*)cond_handle;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

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

        free(cond);
    }
}

/*****************************************************************************
 * ESP-IDF Thread Implementation
 *****************************************************************************/

static int espidf_thread_create(void** handle, void* (*func)(void*), void* arg, size_t stack_size,
                                int priority)
{
    TaskHandle_t task_handle;
    BaseType_t ret;
    int task_priority;

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
    ret = xTaskCreate((TaskFunction_t)func, "usbip_task", stack_size / sizeof(StackType_t), arg,
                      task_priority, &task_handle);

    if (ret != pdPASS)
    {
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

static int espidf_thread_detach(void* handle)
{
    /* In FreeRTOS, tasks are automatically cleaned up when they exit
     * if they were created with xTaskCreate. Nothing special to do here. */
    (void)handle;

    return OSAL_OK;
}

static int espidf_thread_delete(void* handle)
{
    TaskHandle_t task = (TaskHandle_t)handle;

    vTaskDelete(task);

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

    /* Thread */
    .thread_create = espidf_thread_create,
    .thread_join = espidf_thread_join,
    .thread_detach = espidf_thread_detach,
    .thread_delete = espidf_thread_delete,

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
OSAL_REGISTER(espidf, espidf_ops);
