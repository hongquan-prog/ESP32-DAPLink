/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-10-08    windows       Initial version
 * 2026-03-17    lihongquan    Refactored according to code style guide
 */

#include "main/DAP_handle.h"
#include "main/usbip_server.h"
#include "main/dap_configuration.h"
#include "main/wifi_configuration.h"

#include "components/USBIP/usb_descriptor.h"
#include "DAP.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include <stdint.h>
#include <string.h>

#if ((USE_MDNS == 1) || (USE_OTA == 1))
    #define DAP_BUFFER_NUM 10
#else
    #define DAP_BUFFER_NUM 20
#endif

#if (USE_WINUSB == 1)
typedef struct
{
    uint32_t length;
    uint8_t buf[DAP_PACKET_SIZE];
} dap_packet_t;
#else
typedef struct
{
    uint8_t buf[DAP_PACKET_SIZE];
} dap_packet_t;
#endif

#define DAP_HANDLE_SIZE (sizeof(dap_packet_t))

extern int g_k_sock;
static TaskHandle_t s_dap_task_handle = NULL;
static dap_signal_t s_restart_signal = DAP_SIGNAL_NONE;

/**
 * @brief Set DAP restart signal
 *
 * @param signal Signal type
 */
void dap_set_restart_signal(dap_signal_t signal)
{
    s_restart_signal = signal;
}

/**
 * @brief Get current DAP restart signal
 *
 * @return Current signal value
 */
dap_signal_t dap_get_restart_signal(void)
{
    return s_restart_signal;
}

/**
 * @brief Clear DAP restart signal to none
 */
void dap_clear_restart_signal(void)
{
    s_restart_signal = DAP_SIGNAL_NONE;
}

/**
 * @brief Initialize DAP task
 */
void dap_task_init(void)
{
    xTaskCreate(dap_task, "DAP_Task", 8192, NULL, 10, &s_dap_task_handle);
}

/**
 * @brief Get DAP task handle
 *
 * @return Current DAP task handle
 */
TaskHandle_t dap_get_task_handle(void)
{
    return s_dap_task_handle;
}

/**
 * @brief Notify DAP task
 */
void dap_notify_task(void)
{
    if (s_dap_task_handle)
    {
        xTaskNotifyGive(s_dap_task_handle);
    }
}


static dap_packet_t s_dap_data_processed;
static int s_dap_respond = 0;

/* SWO Trace */
static uint8_t *s_swo_data_to_send = NULL;
static uint32_t s_swo_data_num = 0;

/* DAP handle */
static RingbufHandle_t s_dap_data_in_handle = NULL;
static RingbufHandle_t s_dap_data_out_handle = NULL;
static SemaphoreHandle_t s_data_response_mux = NULL;

/**
 * @brief Allocate DAP ring buffers
 */
void malloc_dap_ringbuf(void)
{
    if (s_data_response_mux && xSemaphoreTake(s_data_response_mux, portMAX_DELAY) == pdTRUE)
    {
        if (s_dap_data_in_handle == NULL)
        {
            s_dap_data_in_handle = xRingbufferCreate(DAP_HANDLE_SIZE * DAP_BUFFER_NUM, RINGBUF_TYPE_BYTEBUF);
        }
        if (s_dap_data_out_handle == NULL)
        {
            s_dap_data_out_handle = xRingbufferCreate(DAP_HANDLE_SIZE * DAP_BUFFER_NUM, RINGBUF_TYPE_BYTEBUF);
        }

        xSemaphoreGive(s_data_response_mux);
    }
}

/**
 * @brief Free DAP ring buffers
 */
void free_dap_ringbuf(void)
{
    if (s_data_response_mux && xSemaphoreTake(s_data_response_mux, portMAX_DELAY) == pdTRUE)
    {
        if (s_dap_data_in_handle)
        {
            vRingbufferDelete(s_dap_data_in_handle);
        }
        if (s_dap_data_out_handle)
        {
            vRingbufferDelete(s_dap_data_out_handle);
        }

        s_dap_data_in_handle = NULL;
        s_dap_data_out_handle = NULL;
        xSemaphoreGive(s_data_response_mux);
    }
}


void handle_dap_data_request(usbip_stage2_header *header, uint32_t length)
{
    uint8_t *data_in = (uint8_t *)header;
    data_in = &(data_in[sizeof(usbip_stage2_header)]);

    /* Point to the beginning of the URB packet */
#if (USE_WINUSB == 1)
    usbip_send_stage2_submit(header, 0, 0);

    /* always send constant size buf -> cuz we don't care about the IN packet size */
    /* and to unify the style, we set aside the length of the section */
    xRingbufferSend(s_dap_data_in_handle, data_in - sizeof(uint32_t), DAP_HANDLE_SIZE, portMAX_DELAY);
    dap_notify_task();

#else
    usbip_send_stage2_submit(header, 0, 0);

    xRingbufferSend(s_dap_data_in_handle, data_in, DAP_HANDLE_SIZE, portMAX_DELAY);
    dap_notify_task();

#endif
}

void handle_dap_data_response(usbip_stage2_header *header)
{
    return;
    // int resLength = dap_respond & 0xFFFF;
    // if (resLength)
    // {

    //     usbip_send_stage2_submit_data(header, 0, (void *)DAPDataProcessed.buf, resLength);
    //     dap_respond = 0;
    // }
    // else
    // {
    //     usbip_send_stage2_submit(header, 0, 0);
    // }
}

void handle_swo_trace_response(usbip_stage2_header *header)
{
#if (SWO_FUNCTION_ENABLE == 1)
    if (kSwoTransferBusy)
    {
        /* busy indicates that there is data to be send */
        os_printf("swo use data\r\n");
        usbip_send_stage2_submit_data(header, 0, (void *)s_swo_data_to_send, s_swo_data_num);
        SWO_TransferComplete();
    }
    else
    {
        /* nothing to send */
        usbip_send_stage2_submit(header, 0, 0);
    }
#else
    usbip_send_stage2_submit(header, 0, 0);
#endif
}

/**
 * @brief SWO Data Queue Transfer
 *
 * @param buf Pointer to buffer with data
 * @param num Number of bytes to transfer
 */
void SWO_QueueTransfer(uint8_t *buf, uint32_t num)
{
    s_swo_data_to_send = buf;
    s_swo_data_num = num;
}

void dap_task(void *argument)
{
    size_t packet_size = 0;
    int res_length = 0;
    dap_packet_t *item = NULL;

    s_dap_data_in_handle = xRingbufferCreate(DAP_HANDLE_SIZE * DAP_BUFFER_NUM, RINGBUF_TYPE_BYTEBUF);
    s_dap_data_out_handle = xRingbufferCreate(DAP_HANDLE_SIZE * DAP_BUFFER_NUM, RINGBUF_TYPE_BYTEBUF);
    s_data_response_mux = xSemaphoreCreateMutex();

    if (s_dap_data_in_handle == NULL || s_dap_data_out_handle == NULL ||
        s_data_response_mux == NULL)
    {
        os_printf("Can not create DAP ringbuf/mux!\r\n");
        vTaskDelete(NULL);
    }

    for (;;)
    {
        while (1)
        {
            dap_signal_t signal = dap_get_restart_signal();

            if (signal != DAP_SIGNAL_NONE)
            {
                free_dap_ringbuf();

                if (signal == DAP_SIGNAL_RESET)
                {
                    malloc_dap_ringbuf();
                    if (s_dap_data_in_handle == NULL || s_dap_data_out_handle == NULL)
                    {
                        os_printf("Can not create DAP ringbuf/mux!\r\n");
                        vTaskDelete(NULL);
                    }
                }

                dap_clear_restart_signal();
            }

            ulTaskNotifyTake(pdFALSE, portMAX_DELAY);

            if (s_dap_data_in_handle == NULL || s_dap_data_out_handle == NULL)
            {
                continue;
            }

            packet_size = 0;
            item = (dap_packet_t *)xRingbufferReceiveUpTo(s_dap_data_in_handle, &packet_size,
                                                          pdMS_TO_TICKS(1), DAP_HANDLE_SIZE);
            if (packet_size == 0)
            {
                break;
            }
            else if (packet_size < DAP_HANDLE_SIZE)
            {
                os_printf("Wrong data in packet size:%d , data in remain: %d\r\n", packet_size, (int)xRingbufferGetMaxItemSize(s_dap_data_in_handle));
                vRingbufferReturnItem(s_dap_data_in_handle, (void *)item);
                break;
            }

            if (item->buf[0] == ID_DAP_QueueCommands)
            {
                item->buf[0] = ID_DAP_ExecuteCommands;
            }

            res_length = DAP_ProcessCommand((uint8_t *)item->buf, (uint8_t *)s_dap_data_processed.buf);
            res_length &= 0xFFFF;

            vRingbufferReturnItem(s_dap_data_in_handle, (void *)item);

#if (USE_WINUSB == 1)
            s_dap_data_processed.length = res_length;
#endif
            xRingbufferSend(s_dap_data_out_handle, (void *)&s_dap_data_processed, DAP_HANDLE_SIZE, portMAX_DELAY);

            if (xSemaphoreTake(s_data_response_mux, portMAX_DELAY) == pdTRUE)
            {
                ++s_dap_respond;
                xSemaphoreGive(s_data_response_mux);
            }
        }
    }
}

int fast_reply(uint8_t *buf, uint32_t length)
{
    usbip_stage2_header *buf_header = (usbip_stage2_header *)buf;

    if (length == 48 &&
        buf_header->base.command == PP_HTONL(USBIP_STAGE2_REQ_SUBMIT) &&
        buf_header->base.direction == PP_HTONL(USBIP_DIR_IN) &&
        buf_header->base.ep == PP_HTONL(1))
    {
        if (s_dap_respond > 0)
        {
            dap_packet_t *item = NULL;
            size_t packet_size = 0;

            item = (dap_packet_t *)xRingbufferReceiveUpTo(s_dap_data_out_handle, &packet_size,
                                                          pdMS_TO_TICKS(10), DAP_HANDLE_SIZE);
            if (packet_size == DAP_HANDLE_SIZE)
            {
#if (USE_WINUSB == 1)
                usbip_send_stage2_submit_data_fast((usbip_stage2_header *)buf, item->buf, item->length);
#else
                usbip_send_stage2_submit_data_fast((usbip_stage2_header *)buf, item->buf, DAP_HANDLE_SIZE);
#endif

                if (xSemaphoreTake(s_data_response_mux, portMAX_DELAY) == pdTRUE)
                {
                    --s_dap_respond;
                    xSemaphoreGive(s_data_response_mux);
                }

                vRingbufferReturnItem(s_dap_data_out_handle, (void *)item);

                return 1;
            }
            else if (packet_size > 0)
            {
                os_printf("Wrong data out packet size:%d!\r\n", packet_size);
            }
        }
        else
        {
            buf_header->base.command = PP_HTONL(USBIP_STAGE2_RSP_SUBMIT);
            buf_header->base.direction = PP_HTONL(USBIP_DIR_OUT);
            buf_header->u.ret_submit.status = 0;
            buf_header->u.ret_submit.data_length = 0;
            buf_header->u.ret_submit.error_count = 0;
            usbip_network_send(g_k_sock, buf, 48, 0);

            return 1;
        }
    }

    return 0;
}

void handle_dap_unlink(void)
{
    /* `USBIP_CMD_UNLINK` means calling `usb_unlink_urb()` or `usb_kill_urb()`. */
    /* Note that execution of an URB is inherently an asynchronous operation, and there may be */
    /* synchronization problems in the following solutions. */

    /* One of the reasons this happens is that the host wants to abort the URB transfer operation */
    /* as soon as possible. USBIP network fluctuations will also cause this error, but I don't know */
    /* whether this is the main reason. */

    /* Unlink may be applied to zero length URB of "DIR_IN", or a URB containing data. */
    /* In the case of unlink, for the new "DIR_IN" request, it may always return an older response, */
    /* which will lead to panic. This code is a compromise for eliminating the lagging response */
    /* caused by UNLINK. It will clean up the buffers that may have data for return to the host. */
    /* In general, we assume that there is at most one piece of data that has not yet been returned. */
    if (s_dap_respond > 0)
    {
        dap_packet_t *item = NULL;
        size_t packet_size = 0;

        item = (dap_packet_t *)xRingbufferReceiveUpTo(s_dap_data_out_handle, &packet_size,
                                                      pdMS_TO_TICKS(10), DAP_HANDLE_SIZE);
        if (packet_size == DAP_HANDLE_SIZE)
        {
            if (xSemaphoreTake(s_data_response_mux, portMAX_DELAY) == pdTRUE)
            {
                --s_dap_respond;
                xSemaphoreGive(s_data_response_mux);
            }

            vRingbufferReturnItem(s_dap_data_out_handle, (void *)item);
        }
    }
}