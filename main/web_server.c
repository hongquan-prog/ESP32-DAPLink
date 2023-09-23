/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   add license declaration
 */
#include "web_server.h"
#include <stdbool.h>
#include "esp_log.h"
#include "web_handler.h"

#define TAG "web_server"

static web_data_t s_web_data = {0};
static const httpd_uri_t s_index = {"/", HTTP_GET, web_index_handler, &s_web_data, false, false, NULL};
static const httpd_uri_t s_webserial = {"/webserial", HTTP_GET, web_serial_handler, &s_web_data, true, true, NULL};
static const httpd_uri_t s_webserial_send = {"/webserial_socket", HTTP_GET, web_send_to_uart, &s_web_data, true, true, NULL};
static const httpd_uri_t s_favicon = {"/favicon.ico", HTTP_GET, web_favicon_handler, &s_web_data, false, false, NULL};
static const httpd_uri_t s_get_program = {"/program", HTTP_GET, web_program_handler, &s_web_data, false, false, NULL};
static const httpd_uri_t s_post_program = {"/program", HTTP_POST, web_flash_handler, &s_web_data, false, false, NULL};
static const httpd_uri_t s_query = {"/api/query*", HTTP_GET, web_query_handler, &s_web_data, false, false, NULL};
static const httpd_uri_t s_upload_file = {"/api/upload*", HTTP_POST, web_upload_file_handler, &s_web_data, false, false, NULL};
static const httpd_uri_t s_online_program = {"/api/online-program", HTTP_POST, web_online_program_handler, &s_web_data, false, false, NULL};

bool web_server_init(httpd_handle_t *server)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (*server || s_web_data.server)
    {
        ESP_LOGE(TAG, "Server already started");
        return false;
    }

    config.max_uri_handlers = 12;
    config.max_open_sockets = CONFIG_HTTPD_MAX_OPENED_SOCKETS;
    config.uri_match_fn = httpd_uri_match_wildcard;
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);

    if (httpd_start(&s_web_data.server, &config) != ESP_OK)
    {
        ESP_LOGI(TAG, "Error starting server!");
        return false;
    }

    httpd_register_uri_handler(s_web_data.server, &s_webserial);
    httpd_register_uri_handler(s_web_data.server, &s_webserial_send);
    httpd_register_uri_handler(s_web_data.server, &s_index);
    httpd_register_uri_handler(s_web_data.server, &s_favicon);
    httpd_register_uri_handler(s_web_data.server, &s_get_program);
    httpd_register_uri_handler(s_web_data.server, &s_post_program);
    httpd_register_uri_handler(s_web_data.server, &s_upload_file);
    httpd_register_uri_handler(s_web_data.server, &s_query);
    httpd_register_uri_handler(s_web_data.server, &s_online_program);
    *server = s_web_data.server;

    return true;
}

bool web_server_stop(httpd_handle_t *server)
{
    if (!server || (*server != s_web_data.server))
    {
        ESP_LOGE(TAG, "Invalid server");
        return false;
    }

    if (httpd_stop(*server) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop http server");
        return false;
    }

    ESP_LOGI(TAG, "Stopping webserver");
    *server = NULL;
    s_web_data.server = NULL;

    return true;
}