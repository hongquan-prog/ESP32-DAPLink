/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   add license declaration
 */
#pragma once

#include "esp_http_server.h"

typedef struct
{
    httpd_handle_t server;
    uint8_t buf[CONFIG_HTTPD_RESP_BUF_SIZE];
} web_data_t;

#ifdef __cplusplus
extern "C"
{
#endif

    void web_send_to_clients(void *context, uint8_t *data, size_t size);
    esp_err_t web_serial_handler(httpd_req_t *req);
    esp_err_t web_send_to_uart(httpd_req_t *req);
    esp_err_t web_index_handler(httpd_req_t *req);
    esp_err_t web_favicon_handler(httpd_req_t *req);
    esp_err_t web_program_handler(httpd_req_t *req);
    esp_err_t web_flash_handler(httpd_req_t *req);
    esp_err_t web_upload_file_handler(httpd_req_t *req);
    esp_err_t web_query_handler(httpd_req_t *req);
    esp_err_t web_online_program_handler(httpd_req_t *req);

#ifdef __cplusplus
}
#endif