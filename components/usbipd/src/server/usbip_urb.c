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
 * URB Handler with Static Memory
 *
 * Use static memory pool for URB processing, meeting 10KB memory limit
 */
#include <endian.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "hal/usbip_log.h"
#include "hal/usbip_osal.h"
#include "hal/usbip_transport.h"
#include "usbip_protocol.h"
#include "usbip_server.h"

LOG_MODULE_REGISTER(urb, CONFIG_USBIP_LOG_LEVEL);

/*****************************************************************************
 * Static URB Queue (~2KB)
 *****************************************************************************/

struct urb_slot
{
    struct usbip_header header;
    uint8_t data[USBIP_URB_DATA_MAX_SIZE];
    size_t data_len;
    int valid;
};

struct urb_queue
{
    struct urb_slot slots[USBIP_URB_QUEUE_SIZE];
    int head;
    int tail;
    struct osal_mutex lock;
    struct osal_cond not_empty;
    struct osal_cond not_full;
    int closed;
};

/*****************************************************************************
 * Global URB Queue (Statically Allocated)
 *****************************************************************************/

static struct urb_queue g_urb_queue;

/*****************************************************************************
 * URB Queue Operations
 *****************************************************************************/

static int usbip_urb_queue_init(struct urb_queue* q)
{
    memset(q, 0, sizeof(*q));
    if (osal_mutex_init(&q->lock) != OSAL_OK)
    {
        return -1;
    }

    if (osal_cond_init(&q->not_empty) != OSAL_OK)
    {
        osal_mutex_destroy(&q->lock);
        return -1;
    }

    if (osal_cond_init(&q->not_full) != OSAL_OK)
    {
        osal_cond_destroy(&q->not_empty);
        osal_mutex_destroy(&q->lock);
        return -1;
    }
    return 0;
}

static void usbip_urb_queue_destroy(struct urb_queue* q)
{
    osal_mutex_destroy(&q->lock);
    osal_cond_destroy(&q->not_empty);
    osal_cond_destroy(&q->not_full);
}

static int usbip_urb_queue_push(struct urb_queue* q, const struct usbip_header* header,
                                const void* data, size_t data_len)
{
    if (data_len > USBIP_URB_DATA_MAX_SIZE)
    {
        LOG_ERR("URB data size %zu exceeds max %d", data_len, USBIP_URB_DATA_MAX_SIZE);
        return -1;
    }

    osal_mutex_lock(&q->lock);

    while (((q->tail + 1) % USBIP_URB_QUEUE_SIZE) == q->head && !q->closed)
    {
        osal_cond_wait(&q->not_full, &q->lock);
    }

    if (q->closed)
    {
        osal_mutex_unlock(&q->lock);
        return -1;
    }

    /* Copy data to static slot */
    struct urb_slot* slot = &q->slots[q->tail];
    memcpy(&slot->header, header, sizeof(*header));
    if (data && data_len > 0)
    {
        memcpy(slot->data, data, data_len);
    }

    slot->data_len = data_len;
    slot->valid = 1;
    q->tail = (q->tail + 1) % USBIP_URB_QUEUE_SIZE;
    osal_cond_signal(&q->not_empty);
    osal_mutex_unlock(&q->lock);

    return 0;
}

static int usbip_urb_queue_pop(struct urb_queue* q, struct usbip_header* header, void* data,
                               size_t* data_len)
{
    osal_mutex_lock(&q->lock);

    /* Wait for data */
    while (q->head == q->tail && !q->closed)
    {
        osal_cond_wait(&q->not_empty, &q->lock);
    }

    if (q->head == q->tail)
    {
        osal_mutex_unlock(&q->lock);
        return -1;
    }

    struct urb_slot* slot = &q->slots[q->head];
    memcpy(header, &slot->header, sizeof(*header));
    *data_len = slot->data_len;
    if (slot->data_len > 0 && data)
    {
        memcpy(data, slot->data, slot->data_len);
    }
    slot->valid = 0;

    q->head = (q->head + 1) % USBIP_URB_QUEUE_SIZE;

    osal_cond_signal(&q->not_full);
    osal_mutex_unlock(&q->lock);

    return 0;
}

static void usbip_urb_queue_close(struct urb_queue* q)
{
    osal_mutex_lock(&q->lock);
    q->closed = 1;
    osal_cond_broadcast(&q->not_empty);
    osal_cond_broadcast(&q->not_full);
    osal_mutex_unlock(&q->lock);
}

/*****************************************************************************
 * URB Processing Context
 *****************************************************************************/

struct urb_context
{
    struct usbip_conn_ctx* conn;
    struct usbip_device_driver* driver;
    const char* busid;
    volatile int* running;
    struct urb_queue* queue;
    struct osal_thread processor_thread;
    int processor_started;
};

/*****************************************************************************
 * URB Response Sending
 *****************************************************************************/

static int usbip_urb_send_reply(struct usbip_conn_ctx* ctx, struct usbip_header* urb_ret,
                                const void* data, size_t data_len)
{
    ssize_t n;

    LOG_DBG("Sending URB reply: cmd=%u seq=%u devid=%u dir=%u ep=%u status=%d len=%d",
            urb_ret->base.command, urb_ret->base.seqnum, urb_ret->base.devid,
            urb_ret->base.direction, urb_ret->base.ep, urb_ret->u.ret_submit.status,
            urb_ret->u.ret_submit.actual_length);

    /* Send header first */
    usbip_pack_header(urb_ret, 1);
    n = transport_send(ctx, urb_ret, sizeof(*urb_ret));
    if (n != sizeof(*urb_ret))
    {
        LOG_ERR("send header failed");
        return -1;
    }

    /* Send data (if any) */
    if (data && data_len > 0)
    {
        n = transport_send(ctx, data, data_len);
        if (n != (ssize_t)data_len)
        {
            LOG_ERR("send data failed");
            return -1;
        }
    }

    return sizeof(*urb_ret) + data_len;
}

/*****************************************************************************
 * URB Processing Thread
 *****************************************************************************/

static void* usbip_urb_processor_thread(void* arg)
{
    struct urb_context* ctx = (struct urb_context*)arg;
    struct usbip_header urb_cmd, urb_ret;
    uint8_t urb_data[USBIP_URB_DATA_MAX_SIZE];
    size_t urb_data_len;
    void* data_out = NULL;
    size_t data_len = 0;
    int ret;

    LOG_DBG("URB processor started for %s", ctx->busid);

    while (*ctx->running)
    {
        /* Get URB from queue */
        if (usbip_urb_queue_pop(ctx->queue, &urb_cmd, urb_data, &urb_data_len) < 0)
        {
            break;
        }

        LOG_DBG("Processing URB: cmd=%u seq=%u dir=%u ep=%u", urb_cmd.base.command,
                urb_cmd.base.seqnum, urb_cmd.base.direction, urb_cmd.base.ep);

        /* Call driver to process URB */
        data_out = NULL;
        data_len = 0;
        ret = ctx->driver->handle_urb(ctx->driver, &urb_cmd, &urb_ret, &data_out, &data_len,
                                      urb_data, urb_data_len);

        if (ret < 0)
        {
            LOG_ERR("URB handling error");
            osal_free(data_out);
            break;
        }

        /* Send response */
        if (ret > 0 || urb_cmd.base.direction == USBIP_DIR_OUT)
        {
            if (usbip_urb_send_reply(ctx->conn, &urb_ret, data_out, data_len) < 0)
            {
                LOG_ERR("Failed to send URB reply");
                osal_free(data_out);
                break;
            }
        }

        osal_free(data_out);
    }

    LOG_INF("URB processor exiting for %s", ctx->busid);
    osal_thread_delete(&ctx->processor_thread);

    return NULL;
}

/*****************************************************************************
 * URB Processing Loop
 *****************************************************************************/

int usbip_urb_loop(struct usbip_conn_ctx* ctx, struct usbip_device_driver* driver,
                   const char* busid)
{
    struct urb_context urb_ctx;
    struct usbip_header urb_cmd;
    uint8_t urb_data[USBIP_URB_DATA_MAX_SIZE];
    size_t urb_data_len;
    static volatile int running = 1;

    LOG_DBG("Entering URB loop for %s", busid);

    /* Initialize context */
    memset(&urb_ctx, 0, sizeof(urb_ctx));
    urb_ctx.conn = ctx;
    urb_ctx.driver = driver;
    urb_ctx.busid = busid;
    urb_ctx.running = &running;
    urb_ctx.queue = &g_urb_queue;

    if (usbip_urb_queue_init(&g_urb_queue) < 0)
    {
        LOG_ERR("Failed to init URB queue");
        driver->unexport_device(driver, busid);
        return -1;
    }

    /* 启动处理线程 */
    if (osal_thread_create(&urb_ctx.processor_thread, usbip_urb_processor_thread, &urb_ctx, CONFIG_URB_THREAD_STACK_SIZE, CONFIG_URB_THREAD_PRIORITY) !=
        OSAL_OK)
    {
        usbip_urb_queue_destroy(&g_urb_queue);
        driver->unexport_device(driver, busid);
        return -1;
    }

    urb_ctx.processor_started = 1;
    /* 接收线程：从传输层读取 URB 并入队 */
    while (running)
    {
        int recv_ret = usbip_recv_header(ctx, &urb_cmd);

        if (recv_ret < 0)
        {
            LOG_DBG("Connection closed or error");
            break;
        }

        LOG_DBG("Received URB: cmd=%u seq=%u devid=%u dir=%u ep=%u", urb_cmd.base.command,
                urb_cmd.base.seqnum, urb_cmd.base.devid, urb_cmd.base.direction, urb_cmd.base.ep);

        urb_data_len = 0;
        if (urb_cmd.base.direction == USBIP_DIR_OUT &&
            urb_cmd.u.cmd_submit.transfer_buffer_length > 0)
        {

            urb_data_len = urb_cmd.u.cmd_submit.transfer_buffer_length;
            if (urb_data_len > USBIP_URB_DATA_MAX_SIZE)
            {
                urb_data_len = USBIP_URB_DATA_MAX_SIZE;
            }

            if (transport_recv(ctx, urb_data, urb_data_len) != (ssize_t)urb_data_len)
            {
                LOG_ERR("Failed to receive URB data");
                break;
            }
        }

        if (usbip_urb_queue_push(&g_urb_queue, &urb_cmd, urb_data, urb_data_len) < 0)
        {
            break;
        }
    }

    usbip_urb_queue_close(&g_urb_queue);
    if (urb_ctx.processor_started)
    {
        osal_thread_join(&urb_ctx.processor_thread);
    }

    usbip_urb_queue_destroy(&g_urb_queue);
    driver->unexport_device(driver, busid);

    LOG_INF("Exited URB loop for %s", busid);

    return 0;
}